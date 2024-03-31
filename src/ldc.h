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

#define LOG_RDMA_DATA info

struct RDMAData
{
  RDMAData(BlockCacheConfig block_cache_config_, Configuration ops_config_, int machine_index_, infinity::core::Context *context_, infinity::queues::QueuePairFactory* qp_factory_) :
    block_cache_config(block_cache_config_), ops_config(ops_config_), machine_index(machine_index_), context(context_), qp_factory(qp_factory_)
  {
    for (auto i = 0; i < block_cache_config.remote_machine_configs.size(); i++)
    {
      auto remote_machine_config = block_cache_config.remote_machine_configs[i];
      if (i == machine_index)
      {
        my_server_config = remote_machine_config;
        server_machine_index = server_configs.size();
      }
      if (remote_machine_config.server)
      {
        server_configs.push_back(remote_machine_config);
      }
    }
  }

  void listen(int port, void* buffer, uint64_t size)
  {
    auto& [read_write_buffer, region_token] = get_buffer(buffer, size);
		region_token = read_write_buffer->createRegionToken();

    LOG_RDMA_DATA("[RDMAData] Listening on port [{}:{}]", my_server_config.ip, port);
    qp_factory->bindToPort(port);
    LOG_RDMA_DATA("[RDMAData] Listening on port [{}:{}] bound", my_server_config.ip, port);
    for (int i = 0; i < server_configs.size(); i++)
    {
      LOG_RDMA_DATA("[RDMAData] Accepting incoming connection on port [{}]", port);
      start_accepting_connections = true;
  		infinity::queues::QueuePair* qp = qp_factory->acceptIncomingConnection(region_token, sizeof(infinity::memory::RegionToken));
      LOG_RDMA_DATA("[RDMAData] Accepted incoming connection on port [{}:{}]", my_server_config.ip, port);
      qps.emplace_back(qp);
    }
    is_server = true;
  }

  void connect(int port)
  {
    for (auto i = 0; i < server_configs.size(); i++)
    {
      auto server_config = server_configs[i];
      LOG_RDMA_DATA("[RDMAData] Connecting to remote machine [{}:{}]", server_config.ip, port);
      infinity::queues::QueuePair* qp = qp_factory->connectToRemoteHost(server_config.ip.c_str(), port);
      LOG_RDMA_DATA("[RDMAData] Connected to remote machine [{}:{}]", server_config.ip, port);
      qps.emplace_back(qp);
    }
    is_server = false;
  }

  std::shared_ptr<infinity::requests::RequestToken> read(int remote_index, void* buffer, uint64_t buffer_size, uint64_t local_offset, uint64_t remote_offset, uint64_t size_in_bytes)
  {
    auto& [read_write_buffer, region_token] = get_buffer(buffer, buffer_size);

    auto qp = qps[remote_index];
    region_token = (infinity::memory::RegionToken *)qp->getUserData();
    auto request_token = std::make_shared<infinity::requests::RequestToken>(context);
    qp->read(read_write_buffer, local_offset, region_token, remote_offset, size_in_bytes, infinity::queues::OperationFlags(), request_token.get());
    return request_token;
  }

  std::shared_ptr<infinity::requests::RequestToken> write(int remote_index, void* buffer, uint64_t buffer_size, uint64_t local_offset, uint64_t remote_offset, uint64_t size_in_bytes)
  {
    auto& [read_write_buffer, region_token] = get_buffer(buffer, buffer_size);

    auto qp = qps[remote_index];
    auto request_token = std::make_shared<infinity::requests::RequestToken>(context);
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
  int machine_index;
  infinity::core::Context *context;
  infinity::queues::QueuePairFactory *qp_factory;
  std::vector<RemoteMachineConfig> server_configs;
  int server_machine_index;
  RemoteMachineConfig my_server_config;
  void* buffer{};
  uint64_t size{};
  infinity::queues::QueuePair *qp;
  
  HashMap<void*, RDMABufferAndToken> buffer_map;
  std::vector<infinity::queues::QueuePair*> qps;
  bool is_server;
  bool start_accepting_connections = false;
};

template<typename T>
struct RDMADataWithQueue : public RDMAData
{
  RDMADataWithQueue(BlockCacheConfig block_cache_config, Configuration ops_config, int machine_index, infinity::core::Context *context, infinity::queues::QueuePairFactory* qp_factory, uint64_t queue_size) :
    RDMAData(block_cache_config, ops_config, machine_index, context, qp_factory)
  {
    for (auto i = 0; i < queue_size; i++)
    {
      auto data = std::make_shared<T>();
      DataWithRequestToken data_with_token{std::move(data), nullptr};
      free_queue.enqueue(data_with_token);
    }
  }

  void read(int remote_index, const T& read_data, uint64_t local_offset = 0, uint64_t remote_offset = 0, uint64_t size_in_bytes = sizeof(T))
  {
    DataWithRequestToken data_with_request_token;
    while (!free_queue.try_dequeue(data_with_request_token))
    {
      LOG_RDMA_DATA("Cannot queue read operation");
    }
    *data_with_request_token.data = read_data;

    data_with_request_token.token = RDMAData::read(remote_index, data_with_request_token.data.get(), sizeof(T), local_offset, remote_offset, size_in_bytes);
    pending_read_queue.enqueue(std::move(data_with_request_token));
  }

  template<typename F>
  void execute_pending(F&& f)
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
    std::shared_ptr<T> data;
    std::shared_ptr<infinity::requests::RequestToken> token;
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
  uint64_t key = -1;
  uint64_t key_value_storage_offset = -1; // Into KeyValueStorage
};

using CacheIndexLogEntries = std::vector<CacheIndexLogEntry>;

struct MachineCacheIndexLog
{
  CacheIndexLogEntries cache_index_log_entries;
  CopyableAtomic<uint64_t> index;
};

#define CACHE_INDEX_LOG_PORT 50100
#define KEY_VALUE_STORAGE_PORT 50200
#define CACHE_INDEXES_PORT 50200

struct CacheIndexLogs : public RDMAData
{
  CacheIndexLogs(BlockCacheConfig block_cache_config, Configuration ops_config, int machine_index,
    infinity::core::Context *context,
    infinity::queues::QueuePairFactory* qp_factory) :
    RDMAData(block_cache_config, ops_config, machine_index, context, qp_factory)
  {
    LOG_RDMA_DATA("[CacheIndexLogs] Initializing");
    machine_cache_index_logs.resize(server_configs.size());
    for (auto i = 0; i < server_configs.size(); i++)
    {
      auto server_config = server_configs[i];
      // TODO: add to config
      auto cache_index_log_size = MAX_CACHE_INDEX_LOG_SIZE;
      auto& cache_index_log_entries = machine_cache_index_logs[i].cache_index_log_entries;
      cache_index_log_entries.resize(cache_index_log_size);
      auto done_connect = std::async(std::launch::async, [&] {
        while(!start_accepting_connections)
        {
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        RDMAData::connect(CACHE_INDEX_LOG_PORT + i);
      });
      cache_index_log_entries_size = cache_index_log_entries.size() * sizeof(CacheIndexLogEntry);
      RDMAData::listen(CACHE_INDEX_LOG_PORT + i, cache_index_log_entries.data(), cache_index_log_entries_size);
      done_connect.wait();
    }
    LOG_RDMA_DATA("[CacheIndexLogs] Initialized");
  }

  void append_entry(CacheIndexLogEntry entry)
  {
    for (auto i = 0; i < server_configs.size(); i++)
    {
      auto& [cache_index_log_entries, log_index] = machine_cache_index_logs[i];
      auto current_log_index = log_index.fetch_add(1, std::memory_order_relaxed);
      auto& cache_index_log_entry = cache_index_log_entries[current_log_index];
      cache_index_log_entry = entry;
      
      // Update this on remotes
      auto request_token = RDMAData::write(i, cache_index_log_entries.data(), cache_index_log_entries_size, 0, current_log_index * sizeof(CacheIndexLogEntry), sizeof(CacheIndexLogEntry));      
      pending_write_queue.enqueue(request_token);
    }
  }

  template<typename F>
  void execute_pending(F&& f)
  {
    std::shared_ptr<infinity::requests::RequestToken> token;
    while (pending_write_queue.try_dequeue(token))
    {
      token->waitUntilCompleted();
      f();
    }
  }

  const uint64_t MAX_CACHE_INDEX_LOG_SIZE = 10000;
  uint64_t cache_index_log_entries_size = 0;
  std::vector<MachineCacheIndexLog> machine_cache_index_logs;
  MPMCQueue<std::shared_ptr<infinity::requests::RequestToken>> pending_write_queue;
};

constexpr auto CacheIndexValueQueueSize = 1000;

struct KeyValueStorage : public RDMADataWithQueue<RDMACacheIndexKeyValue>
{
  KeyValueStorage(BlockCacheConfig block_cache_config, Configuration ops_config, int machine_index,
    infinity::core::Context *context,
    infinity::queues::QueuePairFactory* qp_factory, RDMAKeyValueStorage* rdma_kv_storage_ = nullptr) :
    RDMADataWithQueue(block_cache_config, ops_config, machine_index, context, qp_factory, CacheIndexValueQueueSize), rdma_kv_storage(rdma_kv_storage_)
  {
    LOG_RDMA_DATA("[KeyValueStorage] Initializing");

    auto key_value_buffer = rdma_kv_storage->get_key_value_buffer();
    auto key_value_buffer_size = rdma_kv_storage->get_key_value_buffer_size();

    auto done_connect = std::async(std::launch::async, [&] {
      while(!start_accepting_connections)
      {
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      connect(KEY_VALUE_STORAGE_PORT);
    });
    listen(KEY_VALUE_STORAGE_PORT, key_value_buffer, key_value_buffer_size);
    done_connect.wait();

    LOG_RDMA_DATA("[KeyValueStorage] Initialized");
  }

  void read(int remote_index, RDMACacheIndex rdma_cache_index)
  {
    RDMACacheIndexKeyValue read_data;
    auto remote_offset = rdma_cache_index.key_value_ptr_offset;

    RDMADataWithQueue::read(remote_index, read_data, 0, remote_offset);
  }

  RDMAKeyValueStorage* rdma_kv_storage{};
};

struct CacheIndexes : public RDMAData
{
  CacheIndexes(BlockCacheConfig block_cache_config, Configuration ops_config, int machine_index,
    infinity::core::Context *context,
    infinity::queues::QueuePairFactory* qp_factory,
    RDMAKeyValueStorage* rdma_kv_storage_) :
    RDMAData(block_cache_config, ops_config, machine_index, context, qp_factory), rdma_kv_storage(rdma_kv_storage_)
  {
    LOG_RDMA_DATA("[CacheIndexes] Initializing");
    rdma_cache_indexes.resize(server_configs.size());
    for (auto i = 0; i < server_configs.size(); i++)
    {
      auto server_config = server_configs[i];
      RDMACacheIndex* cache_index;
      auto is_local = false;
      if (server_config.index == machine_index)
      {
        cache_index = rdma_kv_storage->get_cache_index_buffer();
      }
      else
      {
        cache_index = rdma_kv_storage->allocate_cache_index();
      }
      rdma_cache_indexes[i] = cache_index;

      auto done_connect = std::async(std::launch::async, [&] {
        while(!start_accepting_connections)
        {
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        RDMAData::connect(CACHE_INDEXES_PORT + i);
      });
      auto size = rdma_kv_storage->get_allocated_cache_index_size();
      RDMAData::listen(CACHE_INDEXES_PORT + i, cache_index, size);
      done_connect.wait();
    }
    LOG_RDMA_DATA("[CacheIndexes] Initialized");
  }

  void write_remote(const std::string& key, const std::string& value)
  {
    auto key_index = std::stoi(key);
    for (auto i = 0; i < server_configs.size(); i++)
    {
      const auto& server_config = server_configs[i];
      if (machine_index == i)
      {
        continue;
      }
      auto size = rdma_kv_storage->get_allocated_cache_index_size();
      auto offset = key_index * sizeof(RDMACacheIndex);
      auto request_token = RDMAData::write(i, rdma_cache_indexes[i], size, offset, offset, sizeof(RDMACacheIndex));      
      pending_write_queue.enqueue(request_token);
    }
  }

  template<typename F>
  void execute_pending(F&& f)
  {
    std::shared_ptr<infinity::requests::RequestToken> token;
    while (pending_write_queue.try_dequeue(token))
    {
      token->waitUntilCompleted();
      f();
    }
  }

  std::vector<RDMACacheIndex*> rdma_cache_indexes;
  MPMCQueue<std::shared_ptr<infinity::requests::RequestToken>> pending_write_queue;
  RDMAKeyValueStorage* rdma_kv_storage{};
};

struct RDMAKeyValueCache : public RDMAData
{
  RDMAKeyValueCache(BlockCacheConfig block_cache_config, Configuration ops_config, int machine_index, infinity::core::Context *context,
    infinity::queues::QueuePairFactory* qp_factory,
    RDMAKeyValueStorage* kv_storage_,
    std::shared_ptr<BlockCache<std::string, std::string>> block_cache_) :
    RDMAData(block_cache_config, ops_config, machine_index, context, qp_factory),
    kv_storage(kv_storage_),
    block_cache(block_cache_),
    cache_index_logs(std::make_unique<CacheIndexLogs>(block_cache_config, ops_config, machine_index, context, qp_factory)),
    cache_indexes(std::make_unique<CacheIndexes>(block_cache_config, ops_config, machine_index, context, qp_factory, kv_storage)),
    key_value_storage(std::make_unique<KeyValueStorage>(block_cache_config, ops_config, machine_index, context, qp_factory, kv_storage))
  {
    LOG_RDMA_DATA("[RDMAKeyValueCache] Initialized");
    auto cache = block_cache->get_cache();
    cache->add_callback_on_write([this](const std::string& key, const std::string& value){
      // Update the cache_indexes on remote nodes
      cache_indexes->write_remote(key, value);
    });
    LOG_RDMA_DATA("[RDMAKeyValueCache] Initialized");
  }

  template<typename F>
  void read(int remote_index, const std::string& key, F&& f)
  {
    auto key_index = std::stoi(key);

    key_value_storage->read(remote_index, RDMACacheIndex{ key_index });
  }

  template<typename F>
  void execute_pending(F&& f)
  {
    cache_index_logs->execute_pending([](){});
    cache_indexes->execute_pending(f);
    key_value_storage->execute_pending([](){});
  }

private:
  std::shared_ptr<BlockCache<std::string, std::string>> block_cache;
  RDMAKeyValueStorage* kv_storage;
  std::unique_ptr<CacheIndexLogs> cache_index_logs;
  std::unique_ptr<CacheIndexes> cache_indexes;
  std::unique_ptr<KeyValueStorage> key_value_storage;
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
  std::shared_ptr<RDMAKeyValueCache> rdma_key_value_cache = nullptr;
};

inline std::optional<std::string> find_nic_containing(std::string_view s)
{
    int32_t numberOfInstalledDevices = 0;
    ibv_device **ibvDeviceList = ibv_get_device_list(&numberOfInstalledDevices);

    int dev_i = 0;
    for (int dev_i = 0; dev_i < numberOfInstalledDevices; dev_i++) {
        ibv_device *dev = ibvDeviceList[dev_i];
        const char *name = ibv_get_device_name(dev);
        if (std::string_view(name).find(s) != std::string::npos)
        {
            return name;
        }
    }
    return std::nullopt;
}

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