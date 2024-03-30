#pragma once

#include "defines.h"

#include "operations.h"
#include "heap.h"
#include "async_rdma.h"

struct RDMABufferAndToken
{
  infinity::memory::Buffer* buffer;
  infinity::memory::RegionToken* region_token;
};

struct RDMAData
{
  RDMAData(BlockCacheConfig block_cache_config_, Configuration ops_config_, infinity::core::Context *context_, infinity::queues::QueuePairFactory* qp_factory_) :
    block_cache_config(block_cache_config_), ops_config(ops_config_), context(context_), qp_factory(qp_factory_)
  {
  }

  void listen(int port, void* buffer, uint64_t size)
  {
    auto& [read_write_buffer, region_token] = get_buffer(buffer, size);
		region_token = read_write_buffer->createRegionToken();

    qp_factory->bindToPort(port);
    for (int i = 0; i < block_cache_config.remote_machine_configs.size(); i++)
    {
  		infinity::queues::QueuePair* qp = qp_factory->acceptIncomingConnection(region_token, sizeof(infinity::memory::RegionToken));
      qps.emplace_back(qp);
    }
    is_server = true;
  }

  void connect(int port)
  {
    for (auto i = 0; i < block_cache_config.remote_machine_configs.size(); i++)
    {
      auto remote_machine_config = block_cache_config.remote_machine_configs[i];
      infinity::queues::QueuePair* qp = qp_factory->connectToRemoteHost(remote_machine_config.ip.c_str(), port);
      qps.emplace_back(qp);
    }
    is_server = false;
  }

  std::unique_ptr<infinity::requests::RequestToken> read(int remote_index, void* buffer, uint64_t buffer_size, uint64_t local_offset, uint64_t remote_offset, uint64_t size_in_bytes)
  {
    auto& [read_write_buffer, region_token] = get_buffer(buffer, buffer_size);

    auto qp = qps[remote_index];
    region_token = (infinity::memory::RegionToken *)qp->getUserData();
    auto request_token = std::make_unique<infinity::requests::RequestToken>(context);
    qp->read(read_write_buffer, local_offset, region_token, remote_offset, size_in_bytes, infinity::queues::OperationFlags(), request_token.get());
    return request_token;
  }

  std::unique_ptr<infinity::requests::RequestToken> write(int remote_index, void* buffer, uint64_t buffer_size, uint64_t local_offset, uint64_t remote_offset, uint64_t size_in_bytes)
  {
    auto& [read_write_buffer, region_token] = get_buffer(buffer, buffer_size);

    auto qp = qps[remote_index];
    auto request_token = std::make_unique<infinity::requests::RequestToken>(context);
    qp->write(read_write_buffer, local_offset, region_token, remote_offset, size_in_bytes, infinity::queues::OperationFlags(), request_token.get());
    return request_token;
  }

  RDMABufferAndToken& get_buffer(void* buffer, uint64_t size)
  {
    auto rdma_buffer_and_token = buffer_map.find(buffer);
    if (rdma_buffer_and_token != std::end(buffer_map))
    {
      return rdma_buffer_and_token->second;
    }
    else
    {
      auto read_write_buffer = new infinity::memory::Buffer(context, buffer, size);
      auto [new_buffer, inserted] = buffer_map.insert({buffer, {read_write_buffer, nullptr}});
      return new_buffer->second;
    }
  }

  BlockCacheConfig block_cache_config;
  Configuration ops_config;
  infinity::core::Context *context;
  infinity::queues::QueuePairFactory *qp_factory;
  void* buffer{};
  uint64_t size{};
  infinity::queues::QueuePair *qp;
  
  HashMap<void*, RDMABufferAndToken> buffer_map;
  std::vector<infinity::queues::QueuePair*> qps;
  bool is_server;
};

template<typename T>
struct RDMADataWithQueue : public RDMAData
{
  RDMADataWithQueue(BlockCacheConfig block_cache_config, Configuration ops_config, infinity::core::Context *context, infinity::queues::QueuePairFactory* qp_factory, uint64_t queue_size) :
    RDMAData(block_cache_config, ops_config, context, qp_factory)
  {
    for (auto i = 0; i < queue_size; i++)
    {
      auto data = std::make_unique<T>();
      DataWithRequestToken data_with_token{std::move(data), nullptr};
      free_queue.enqueue(data_with_token);
    }
  }

  void read(int remote_index, const T& read_data, uint64_t local_offset = 0, uint64_t remote_offset = 0, uint64_t size_in_bytes = sizeof(T))
  {
    DataWithRequestToken data_with_request_token;
    while (!free_queue.try_dequeue(data_with_request_token))
    {
      info("Cannot queue read operation");
    }
    *data_with_request_token.data = read_data;

    data_with_request_token.token = RDMAData::read(remote_index, data_with_request_token.data.get(), sizeof(T), local_offset, remote_offset, size_in_bytes);
    pending_read_queue.enqueue(std::move(data_with_request_token));
  }

  template<typename F>
  void execute_pending_reads(F&& f)
  {
    DataWithRequestToken data_with_request_token;
    while (pending_read_queue.try_dequeue(data_with_request_token))
    {
      data_with_request_token.token->waitUntilCompleted();
      f(*data_with_request_token.data);
      free_queue.push(std::move(data_with_request_token));
    }
  }

public:
  struct DataWithRequestToken
  {
    std::unique_ptr<T> data;
    std::unique_ptr<infinity::requests::RequestToken> request_token;
  };

protected:
  MPMCQueue<DataWithRequestToken> pending_read_queue;
  MPMCQueue<DataWithRequestToken> free_queue;
};

struct RDMACacheIndex2
{
  RDMACacheIndex* cache_index;
  bool is_local = false;
};

constexpr auto RDMA_CACHE_INDEX_KEY_VALUE_SIZE = 100;
constexpr auto RDMA_CACHE_INDEX_KEY_QUEUE_SIZE = 100;

struct RDMACacheIndexKeyValue
{
  uint64_t key_index;
  uint8_t data[RDMA_CACHE_INDEX_KEY_VALUE_SIZE];
};

// This index storage maintains a cache index for each remote machine
// This index storage is mapped as rdma to each of the machines, this means we can read directly into the key value storage using the key_value_offset
// Three RDMA points
// [Local CacheIndex] has key_value_offset --> RDMA read --> [KeyValueStore]
// [UpdateCacheIndexLog] has batch updates --> RDMA write --> [Local CacheIndexLog] --> apply local --> [Local CacheIndex]
// Every server -> CacheIndexLog * N servers
// Every server -> KeyValueStore

struct CacheIndexLogEntry
{
  uint64_t key;
  uint64_t key_value_storage_offset; // Into KeyValueStorage
};

using CacheIndexLogEntries = std::vector<CacheIndexLogEntry>;

struct CacheIndexLogs : public RDMAData
{
  CacheIndexLogs(BlockCacheConfig block_cache_config, Configuration ops_config, infinity::core::Context *context, infinity::queues::QueuePairFactory* qp_factory) :
    RDMAData(block_cache_config, ops_config, context, qp_factory)
  {
    cache_index_log_entries_per_machine.resize(block_cache_config.remote_machine_configs.size());
    for (auto i = 0; i < block_cache_config.remote_machine_configs.size(); i++)
    {
      auto remote_machine_config = block_cache_config.remote_machine_configs[i];
      // TODO: add to config
      auto cache_index_log_size = 10000;
      auto& cache_index_log_entries = cache_index_log_entries_per_machine[i];
      cache_index_log_entries.resize(cache_index_log_size);
      RDMAData::listen(remote_machine_config.port, cache_index_log_entries.data(), cache_index_log_entries.size() * sizeof(CacheIndexLogEntry));
      RDMAData::connect(remote_machine_config.port);
    }
    info("CacheIndexLogs initialized");
  }

  std::vector<CacheIndexLogEntries> cache_index_log_entries_per_machine;
};

struct RDMACacheIndexStorage : public RDMAData
{
  RDMACacheIndexStorage(BlockCacheConfig block_cache_config, Configuration ops_config, infinity::core::Context *context, infinity::queues::QueuePairFactory* qp_factory,
    RDMAKeyValueStorage* kv_storage_, int machine_index_) :
    RDMAData(block_cache_config, ops_config, context, qp_factory), kv_storage(kv_storage_), machine_index(machine_index_)
  {
    for (auto i = 0; i < block_cache_config.remote_machine_configs.size(); i++)
    {
      auto remote_machine_config = block_cache_config.remote_machine_configs[i];
      RDMACacheIndex* cache_index;
      auto is_local = false;
      if (remote_machine_config.index == machine_index)
      {
        is_local = true;
      }
      if (is_local)
      {
        cache_index = kv_storage->get_cache_index_buffer();
      }
      else
      {
        cache_index = kv_storage->allocate_cache_index();
      }
      RDMACacheIndex2 rdma_cache_index{ cache_index, is_local };
      rdma_cache_indexes.push_back(rdma_cache_index);
    }
  }

  void listen(int port)
  {
    auto* buffer = kv_storage->get_cache_index_buffer();
    auto size = kv_storage->get_key_value_size();
    RDMAData::listen(port, buffer, size);
  }

  void connect(int port)
  {
    RDMAData::connect(port);
  }

  std::vector<uint8_t> read_value(int remote_index, uint64_t key_index, bool remote_read)
  {
    auto [rdma_cache_index, is_local] = rdma_cache_indexes[remote_index];
    if (is_local)
    {
      panic("Local read not supported");
    }

    auto rdma_cache_index_key = rdma_cache_index[key_index];
    auto key_value_size = kv_storage->get_key_value_size();

    std::vector<uint8_t> buffer(key_value_size);
    auto request_token = RDMAData::read(remote_index, buffer.data(), 0, 0, rdma_cache_index_key.key_value_ptr_offset, key_value_size);
    request_token->waitUntilCompleted();
    return buffer;
  }

  RDMAKeyValueStorage* kv_storage;
  std::vector<RDMACacheIndex2> rdma_cache_indexes;
  int machine_index = -1;
};

struct RDMA_connect
{
  std::string ip;
  u_int64_t index;
  infinity::core::Context *context;
  infinity::queues::QueuePair *qp;
  infinity::queues::QueuePairFactory *qp_factory;
  infinity::memory::RegionToken *remote_buffer_token;
  infinity::memory::Buffer *buffer;
  void *tmp_buffer;
  u_int64_t free_blocks;
  bool isLocal;
  void *local_memory_region;
  AsyncRdmaOpCircularBuffer *circularBuffer;
  std::shared_ptr<BlockCache<std::string, std::string>> block_cache{};
  std::shared_ptr<RDMACacheIndexStorage> rdma_cache_index_storage = nullptr;
};

std::vector<std::string> load_database(Configuration &ops_config,
                                       Client &client);
int generateDatabaseAndOperationSet(Configuration &ops_config);
void executeOperations(
    const std::vector<std::pair<std::string, int>> &operationSet, int client_start_index,
    BlockCacheConfig config, Configuration &ops_config, Client &client, int client_index_per_thread, int machine_index);

void dump_per_thread_latency_to_file(const std::vector<long long> &timestamps, int client_index_per_thread, int machine_index, int thread_index);
void dump_latency_to_file(const std::string &filename, const std::vector<long long> &timestamps);

int issueOps(BlockCacheConfig config, Configuration &ops_config,
             std::vector<std::string> &keys, Client client);

HashMap<uint64_t, RDMA_connect> connect_to_servers(
    BlockCacheConfig config, int machine_index, int value_size, Configuration ops_config, std::shared_ptr<BlockCache<std::string, std::string>> block_cache);

void printRDMAConnect(const RDMA_connect &conn);

void *RDMA_Server_Init(int serverport, uint64_t buffer_size, int machine_index, Configuration ops_config);

void rdma_writer_thread(RDMA_connect &node, uint64_t offset, std::span<uint8_t> buffer_data);

void rdma_reader_thread(RDMA_connect &node, uint64_t offset, void *buffer_data, Server *server, int remoteIndex, int remote_port);

int calculateNodeAndOffset(Configuration &ops_config, uint64_t key, int &nodeIndex, uint64_t &offset);

void write_correct_node(Configuration &ops_config, HashMap<uint64_t, RDMA_connect> &rdma_nodes, int node, uint64_t key, std::span<uint8_t> buffer_data);

void read_correct_node(Configuration &ops_config, HashMap<uint64_t, RDMA_connect> &rdma_nodes, int node, uint64_t key, void *buffer_data, Server *server, int remoteIndex, int remote_port);

void printBuffer(const std::array<uint8_t, BLKSZ> &buffer);

inline void printBlockDBConfig(const BlockDBConfig &config)
{
  std::cout << "BlockDBConfig: { filename: " << config.filename
            << ", num_entries: " << config.num_entries
            << ", block_size: " << config.block_size << " }" << std::endl;
}

inline void printDBConfig(const DBConfig &config)
{
  std::cout << "DBConfig: { block_db: " << std::endl;
  printBlockDBConfig(config.block_db);
  std::cout << "}" << std::endl;
}

inline void printLRUConfig(const LRUConfig &config)
{
  std::cout << "LRUConfig: { cache_size: " << config.cache_size << " }" << std::endl;
}

inline void printRandomCacheConfig(const RandomCacheConfig &config)
{
  std::cout << "RandomCacheConfig: { cache_size: " << config.cache_size << " }" << std::endl;
}

inline void printSplitCacheConfig(const SplitCacheConfig &config)
{
  std::cout << "SplitCacheConfig: { cache_size: " << config.cache_size
            << ", owning_ratio: " << config.owning_ratio
            << ", nonowning_ratio: " << config.nonowning_ratio
            << ", owning_cache_type: " << config.owning_cache_type
            << ", nonowning_cache_type: " << config.nonowning_cache_type << " }" << std::endl;
}

inline void printThreadSafeLRUConfig(const ThreadSafeLRUConfig &config)
{
  std::cout << "ThreadSafeLRUConfig: { cache_size: " << config.cache_size << " }" << std::endl;
}

inline void printRdmaConfig(const RdmaConfig &config)
{
  std::cout << "RdmaConfig: { context_index: " << config.context_index << " }" << std::endl;
}

inline void printCacheConfig(const CacheConfig &config)
{
  std::cout << "CacheConfig: {" << std::endl;
  std::cout << "  LRU: " << std::endl;
  printLRUConfig(config.lru);
  std::cout << "  Random: " << std::endl;
  printRandomCacheConfig(config.random);
  std::cout << "  Split: " << std::endl;
  printSplitCacheConfig(config.split);
  std::cout << "  ThreadSafeLRU: " << std::endl;
  printThreadSafeLRUConfig(config.thread_safe_lru);
  std::cout << "  paged: " << (config.paged ? "true" : "false") << std::endl;
  std::cout << "  RDMA: " << std::endl;
  printRdmaConfig(config.rdma);
  std::cout << "}" << std::endl;
}

inline void printRemoteMachineConfig(const RemoteMachineConfig &config)
{
  std::cout << "RemoteMachineConfig: { index: " << config.index
            << ", ip: " << config.ip
            << ", port: " << config.port
            << ", server: " << (config.server ? "true" : "false") << " }" << std::endl;
}

inline void printBaseline(const Baseline &config)
{
  std::cout << "Baseline: { selected: " << config.selected
            << ", one_sided_rdma_enabled: " << (config.one_sided_rdma_enabled ? "true" : "false") << " }" << std::endl;
}

inline void printBlockCacheConfig(const BlockCacheConfig &config)
{
  std::cout << "BlockCacheConfig: {" << std::endl
            << "  ingest_block_index: " << (config.ingest_block_index ? "true" : "false") << std::endl
            << "  policy_type: " << config.policy_type << std::endl
            << "  rdma_port: " << config.rdma_port << std::endl
            << "  db_type: " << config.db_type << std::endl;
  std::cout << "  DBConfig: " << std::endl;
  printDBConfig(config.db);
  std::cout << "  CacheConfig: " << std::endl;
  printCacheConfig(config.cache);
  std::cout << "  Baseline: " << std::endl;
  printBaseline(config.baseline);
  std::cout << "  RemoteMachineConfigs: " << std::endl;
  for (const auto &remoteConfig : config.remote_machine_configs)
  {
    printRemoteMachineConfig(remoteConfig);
  }
  std::cout << "}" << std::endl;
}