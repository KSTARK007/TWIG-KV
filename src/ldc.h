#pragma once

#include "defines.h"

#include "operations.h"
#include "heap.h"
#include "async_rdma.h"

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
    BlockCacheConfig config, int machine_index, int value_size);

void printRDMAConnect(const RDMA_connect &conn);

void *RDMA_Server_Init(int serverport, uint64_t buffer_size, int machine_index);

void rdma_writer_thread(RDMA_connect &node, uint64_t offset, std::span<uint8_t> buffer_data);

void rdma_reader_thread(RDMA_connect &node, uint64_t offset, void *buffer_data, Server *server, int remoteIndex);

int calculateNodeAndOffset(Configuration &ops_config, uint64_t key, int &nodeIndex, uint64_t &offset);

void write_correct_node(Configuration &ops_config, HashMap<uint64_t, RDMA_connect> &rdma_nodes, int node, uint64_t key, std::span<uint8_t> buffer_data);

void read_correct_node(Configuration &ops_config, HashMap<uint64_t, RDMA_connect> &rdma_nodes, int node, uint64_t key, void *buffer_data, Server *server, int remoteIndex);

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