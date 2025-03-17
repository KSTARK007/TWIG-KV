#include "ldc.h"

DEFINE_string(config, "", "JSON config");
DEFINE_string(dataset_config, "",
              "JSON config for operation parameter for caching");
DEFINE_int64(machine_index, 0, "Index of machine");
DEFINE_int64(threads, 1, "Number of threads");
DEFINE_int64(clients_per_threads, 1, "Number of clients per threads");
DEFINE_string(metrics_path, "metrics.json", "Path to store metrics");
DEFINE_string(cache_dump_path, "cache_dump.txt", "Path to store cache dump");
DEFINE_string(cache_metrics_path, "cache_metrics.json",
              "Path to store cache metrics");
DEFINE_bool(dump_operations, false, "This is to dump the operations");
DEFINE_string(load_dataset, "", "Load dataset from path into store");

inline static std::string default_value;

// Background log for total amount of logs executed
std::atomic<uint64_t> total_ops_executed;

// Background log for total amount of rdmas executed
std::atomic<uint64_t> total_rdma_executed;

// Total disks executed
std::atomic<uint64_t> total_disk_ops_executed;

// Remote disks access
std::atomic<uint64_t> remote_disk_access;

// Local disks access
std::atomic<uint64_t> local_disk_access;

// Timer for disk
uint64_t cache_ns;
uint64_t disk_ns;
uint64_t rdma_ns;

#ifdef CLIENT_SYNC_WITH_OTHER_CLIENTS
// Total clients ready/done for syncing clients with workloads
std::atomic<uint64_t> total_clients_ready{};
std::atomic<bool> workload_ready = false;

std::atomic<uint64_t> total_clients_done{};
std::atomic<bool> workload_done = false;
#endif

std::vector<uint64_t> client_thread_ops_executed;

std::shared_ptr<Snapshot> snapshot = nullptr;

void exec(std::string command, bool print_output = true)
{
  // set up file redirection
  std::filesystem::path redirection = std::filesystem::absolute(".output.temp");
  // command.append(" &> \"" + redirection.string() + "\" 2>&1");
  command.append(" > /dev/null 2>&1");

  // execute command
  auto status = std::system(command.c_str());
}

struct MachnetSync
{
  std::mutex m;
  std::condition_variable cv;
  bool ready = false;
} machnet_sync;

void exec_machnet(const char *cmd)
{
  std::array<char, 128> buffer;
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
  if (!pipe)
  {
    throw std::runtime_error("popen() failed!");
  }
  auto found_status = false;
  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) !=
         nullptr)
  {
    std::string_view s(buffer.data(), buffer.size());

    if (!found_status && s.find("Machnet Engine Status") != std::string::npos)
    {
      std::lock_guard lk(machnet_sync.m);
      machnet_sync.ready = true;
      machnet_sync.cv.notify_one();
      found_status = true;
    }
  }
}

template <typename T>
std::vector<T> get_chunk(std::vector<T> const &vec, std::size_t n, std::size_t i)
{
  assert(i < n);
  std::size_t const q = vec.size() / n;
  std::size_t const r = vec.size() % n;

  auto begin = vec.begin() + i * q + std::min(i, r);
  auto end = vec.begin() + (i + 1) * q + std::min(i + 1, r);

  return std::vector<T>(begin, end);
}

void execute_operations(Client &client, const Operations &operation_set, int client_start_index, BlockCacheConfig config, Configuration &ops_config,
                        int client_index_per_thread, int machine_index, int thread_index)
{
#ifdef CLIENT_SYNC_WITH_OTHER_CLIENTS
  total_clients_ready.fetch_add(1, std::memory_order::relaxed);
  while (total_clients_ready.load(std::memory_order::relaxed) < FLAGS_threads * FLAGS_clients_per_threads)
  {
    std::this_thread::yield();
  }

  // First client is used to sync with other clients
  if (thread_index == 0 && client_index_per_thread == 0)
  {
    client.connect_to_other_clients();
    client.sync_with_other_clients();
    workload_ready.store(true, std::memory_order::relaxed);
  }

  while (!workload_ready.load(std::memory_order::relaxed))
  {
    std::this_thread::yield();
  }
#endif

  auto client_index = (thread_index * FLAGS_clients_per_threads) + client_index_per_thread;

  int wrong_value = 0;
  std::string value;
  std::vector<long long> timeStamps;
  bool dump_latency = false;
  for (int j = 0; j < ops_config.VALUE_SIZE; j++)
  {
    value += static_cast<char>('A');
  }
  long long run_time = 0;
  auto op_start = std::chrono::high_resolution_clock::now();
  auto now = std::chrono::high_resolution_clock::now();
  auto op_end = now - op_start;
  do
  {
    auto io_start = std::chrono::high_resolution_clock::now();
    for (const auto &operation : operation_set)
    {
      io_start = std::chrono::high_resolution_clock::now();
      const auto &[key, index, op] = operation;
      if (index > ops_config.NUM_NODES)
      {
        panic("Invalid node number {}", index);
      }
      LOG_STATE("[{}] [{}] Client executing [{}] [{}]", machine_index, client_index, key, index);
      std::string v = client.get(index + client_start_index, thread_index, key);
      LOG_STATE("[{}] [{}] Client received [{}] [{}]", machine_index, client_index, key, v);
      if (v != value)
      {
        wrong_value++;
        LOG_STATE("[{}] unexpected data {} {} for key {} wrong_values till now {}", index, v, value, key, wrong_value);
      }
      auto now = std::chrono::high_resolution_clock::now();
      auto elapsed = now - io_start;
      long long nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();
      if (dump_latency)
      {
        timeStamps.push_back(nanoseconds);
      }
      total_ops_executed.fetch_add(1, std::memory_order::relaxed);
      client_thread_ops_executed[client_index]++;
      now = std::chrono::high_resolution_clock::now();
      op_end = now - op_start;
      run_time = std::chrono::duration_cast<std::chrono::seconds>(op_end).count();
      if (!dump_latency && run_time >= WARMUP_TIME_IN_SECONDS)
      {
        dump_latency = true;
      }
      if (run_time >= ops_config.TOTAL_RUNTIME_IN_SECONDS + WARMUP_TIME_IN_SECONDS)
      {
        break;
      }
    }
    now = std::chrono::high_resolution_clock::now();
    op_end = now - op_start;
    run_time = std::chrono::duration_cast<std::chrono::seconds>(op_end).count();
  } while (run_time < ops_config.TOTAL_RUNTIME_IN_SECONDS);
  info("[{}] [{}] Client done executing {}", machine_index, client_index, timeStamps.size());
  dump_per_thread_latency_to_file(timeStamps, client_index_per_thread, machine_index, thread_index);
  info("wrong_values till now {}", wrong_value);

#ifdef CLIENT_SYNC_WITH_OTHER_CLIENTS
  total_clients_done.fetch_add(1, std::memory_order::relaxed);
  while (total_clients_done.load(std::memory_order::relaxed) < FLAGS_threads * FLAGS_clients_per_threads)
  {
    std::this_thread::yield();
  }
  if (thread_index == 0 && client_index_per_thread == 0)
  {
    client.sync_with_other_clients();
    workload_done.store(true, std::memory_order::relaxed);
  }

  while (!workload_done.load(std::memory_order::relaxed))
  {
    std::this_thread::yield();
  }
#endif
}

void client_worker(std::shared_ptr<Client> client_, BlockCacheConfig config, Configuration ops_config,
                   int machine_index, int thread_index, Operations ops,
                   int client_index_per_thread)
{
  auto &client = *client_;

  // Find the client index to start from
  auto start_client_index = 0;
  for (auto i = 0; i < config.remote_machine_configs.size(); i++)
  {
    auto remote_machine_config = config.remote_machine_configs[i];
    if (remote_machine_config.server)
    {
      break;
    }
    start_client_index++;
  }

  auto client_index = (thread_index * FLAGS_clients_per_threads) + client_index_per_thread;
  Operations ops_chunk;
  if (ops_config.DISTRIBUTION_TYPE != DistributionType::YCSB)
  {
    ops_chunk = get_chunk(ops, FLAGS_threads * FLAGS_clients_per_threads, client_index);
  }
  else
  {
    std::string file_name = "/mydata/client_" + std::to_string(machine_index) + "_thread_" + std::to_string(thread_index) + "_clientPerThread_" + std::to_string(client_index_per_thread) + ".txt";
    info("Using {} file for operations on client {} thread {} clientPerThread {}", file_name, machine_index, thread_index, client_index_per_thread);
    ops_chunk = loadOperationSetFromFile(file_name);
  }

  info("[{}] [{}] Client executing ops {}", machine_index, client_index, ops_chunk.size());

  auto total_cores = std::thread::hardware_concurrency();
  auto bind_to_core = client_index % total_cores;
  bind_this_thread_to_core(bind_to_core);

  execute_operations(client, ops_chunk, start_client_index - 1, config, ops_config, client_index_per_thread, machine_index, thread_index);
}

void server_worker(
    std::shared_ptr<Server> server_, BlockCacheConfig config, Configuration ops_config, int machine_index,
    int thread_index,
    std::shared_ptr<BlockCache<std::string, std::string>> block_cache,
    HashMap<uint64_t, RDMA_connect> rdma_nodes)
{
  bind_this_thread_to_core(thread_index);
  auto &server = *server_;

  void *read_buffer = malloc(BLKSZ);
  int num_servers = 0;

  int server_start_index;
  for (auto i = 0; i < config.remote_machine_configs.size(); i++)
  {
    // std::cout <<"i " << i << " config.remote_machine_configs[i].index " << config.remote_machine_configs[i].index << std::endl;
    if (config.remote_machine_configs[i].server)
    {
      server_start_index = config.remote_machine_configs[i].index;
      break;
    }
  }

  std::vector<RemoteMachineConfig> server_configs;
  for (auto j = 0; j < config.remote_machine_configs.size(); j++)
  {
    // std::cout <<"i " << i << " config.remote_machine_configs[i].index " << config.remote_machine_configs[i].index << std::endl;
    if (config.remote_machine_configs[j].server)
    {
      server_configs.push_back(config.remote_machine_configs[j]);
      num_servers++;
    }
  }

  auto &rdma_node = std::begin(rdma_nodes)->second;
  if (false && thread_index == 0)
  {
    // Handle if singletons exist on other servers
    block_cache->get_cache()->add_callback_on_write([=, server = server_, &rdma_nodes](const std::string &key, const std::string &value)
                                                    {
      auto& rdma_node = std::begin(rdma_nodes)->second;
      auto key_index = std::stoi(key);

      auto cache_indexes = rdma_node.rdma_key_value_cache->get_cache_indexes();
      auto underlying_cache_indexes = cache_indexes->get_cache_indexes();

      for (auto i = 0; i < underlying_cache_indexes.size(); i++)
      {
        if (i == machine_index - server_start_index)
        {
          continue;
        }
        auto& config = server_configs[i];

        auto& cache_index = underlying_cache_indexes[i];
        // If other key is singleton, delete it
        auto& e = cache_index[key_index];
        if (e.key_value_ptr_offset != KEY_VALUE_PTR_INVALID)
        {
          if (e.isSingleton)
          {
            auto port = config.port;
            server->append_delete_request(i, port, key);
          }
        }
      } });
  }

  auto start_time = std::chrono::high_resolution_clock::now();

  while (!g_stop)
  {
    server.loop(
        [&](auto remote_index, auto remote_port, MachnetFlow &tx_flow, auto &&data)
        {
          if (data.isPutRequest())
          {
            auto p = data.getPutRequest();
            block_cache->put(p.getKey().cStr(), p.getValue().cStr());
            block_cache->get_cache()->update_frequency(p.getKey().cStr());

            server.put_response(remote_index, ResponseType::OK);
          }
          else if (data.isGetRequest())
          {
            auto p = data.getGetRequest();
            auto time_now = std::chrono::high_resolution_clock::now();
            auto elapsed = time_now - start_time;
            auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
            if (elapsed_seconds == WARMUP_TIME_IN_SECONDS)
            {
              block_cache->reset_cache_info();
              local_disk_access.store(0);
              remote_disk_access.store(0);
              total_disk_ops_executed.store(0);
              total_ops_executed.store(0);
              total_rdma_executed.store(0);
            }

            total_ops_executed.fetch_add(1, std::memory_order::relaxed);

            auto key_ = p.getKey();
            auto key = key_.cStr();
            auto key_index = std::stoi(key);
            auto exists_in_cache = block_cache->exists_in_cache(key);
            block_cache->get_cache()->update_frequency(key);

            if (exists_in_cache)
            {
              snapshot->update_cache_hits(key_index);
              // Return the correct key in local cache
              LDCTimer cache_timer;
              auto value = block_cache->get(key, false, exists_in_cache);
              cache_ns = cache_timer.time_elapsed();
              server.get_response(remote_index, remote_port, ResponseType::OK, value);
            }
            else
            {
              block_cache->increment_cache_miss();
              snapshot->update_cache_miss(key_index);
              // Otherwise, if RDMA is renabled, read from the correct node
              bool found_in_rdma = false;

              auto division_of_key_value_pairs = static_cast<float>(ops_config.NUM_KEY_VALUE_PAIRS) / num_servers;
              auto remote_machine_index_to_rdma = static_cast<int>(
                  static_cast<float>(key_index) / division_of_key_value_pairs);

              auto base_index = machine_index - server_start_index;

              auto fetch_from_disk = [=, skey = std::string(key), server = server_](bool add_to_cache)
              {
                snapshot->update_disk_access(key_index);
                std::string value;
                if (ops_config.operations_pollute_cache && add_to_cache)
                {
                  if (ops_config.DISK_ASYNC)
                  {
                    // Cache miss
                    LDCTimer disk_timer;
                    block_cache->get_db()->get_async(skey, [block_cache, server, remote_index, remote_port, skey, disk_timer](auto value)
                                                     {
                      disk_ns = disk_timer.time_elapsed();
                      
                      // Add to cache
                      block_cache->get_cache()->put(skey, value);

                      // Send the response
                      server->append_to_rdma_block_cache_request_queue(remote_index, remote_port, ResponseType::OK, skey, value); });
                  }
                  else
                  {
                    LOG_STATE("Fetching from cache/disk {} {}", skey, value);
                    value = block_cache->get(skey);
                  }
                }
                else
                {
                  // Cache miss
                  LOG_STATE("Fetching from disk {} {}", skey, value);
                  if (ops_config.DISK_ASYNC)
                  {
                    LDCTimer disk_timer;
                    block_cache->get_db()->get_async(skey, [block_cache, server, remote_index, remote_port, skey, disk_timer, add_to_cache](auto value)
                                                     {
                      disk_ns = disk_timer.time_elapsed();
                      
                    if (add_to_cache) {
                      if(std::stoull(skey) >= key_min && std::stoull(skey) < key_max){
                        block_cache->get_cache()->put(skey, value);
                      }
                    }
                      // Send the response
                      server->append_to_rdma_block_cache_request_queue(remote_index, remote_port, ResponseType::OK, skey, value); });
                  }
                  else
                  {
                    if (auto result_or_err = block_cache->get_db()->get(skey))
                    {
                      value = result_or_err.value();
                    }
                    else
                    {
                      panic("Failed to get value from db for key {}", skey);
                    }
                  }
                }

                total_disk_ops_executed.fetch_add(1, std::memory_order::relaxed);
                if (!ops_config.DISK_ASYNC)
                {
                  server->get_response(remote_index, remote_port, ResponseType::OK, value);
                }
              };

              if (config.baseline.one_sided_rdma_enabled)
              {
                if (!config.ingest_block_index)
                {
                  panic("Supports only ingest_block_index being enabled!");
                }

                LOG_STATE("[{}] Reading remote index {}", machine_index, remote_machine_index_to_rdma);
                if (config.baseline.use_cache_indexing)
                {
                  LDCTimer rdma_timer;
                  found_in_rdma = rdma_node.rdma_key_value_cache->read_callback(key_index, [=, expected_key = key_index](const RDMACacheIndexKeyValue &kv)
                                                                                {
                    auto& server = *server_;
                    total_rdma_executed.fetch_add(1, std::memory_order::relaxed);

                    rdma_ns = rdma_timer.time_elapsed();

                    uint64_t key_index = kv.key_index;
                    auto value_view = std::string_view((const char*)kv.data, ops_config.VALUE_SIZE);
                    std::string value(value_view);
                    LOG_RDMA_DATA("[Read RDMA Callback] [{}] key {} value {}", remote_index, key_index, value);
                    if (key_index == expected_key)
                    {
                      LOG_RDMA_DATA("[Read RDMA Callback] Expected! key {} value {}", key_index, value);
                      if(config.policy_type == "nchance"){
                        int remote_index_to_forward = ((base_index + 1) % num_servers) + server_start_index;
                        LOG_RDMA_DATA("remote_index_to_forward {} base_index {} server_start_index {}", 
                        remote_index_to_forward, base_index, server_start_index);
                        auto tmp_ptr = block_cache->get_cache()->put_nchance(std::to_string(key_index), value);

                        if (tmp_ptr != nullptr){
                          LOG_RDMA_DATA("singleton forward to index {} from index {} key {} value {} to cache", remote_index_to_forward, base_index, key_index, value);
                          auto tmp_data = static_cast<EvictionCallbackData<std::string, std::string> *>(tmp_ptr);
                          auto port = config.remote_machine_configs[machine_index].port + thread_index;
                          LOG_RDMA_DATA("Singleton put request key = {} singleton = {} forward_count = {} remote_port = {}",
                              tmp_data->key, tmp_data->singleton, tmp_data->forward_count, port);

                          server.append_singleton_put_request(remote_index_to_forward, port, tmp_data->key, tmp_data->value, tmp_data->singleton, tmp_data->forward_count);
                          delete tmp_data;
                        }
                      }
                      if(config.policy_type == "access_rate" or config.policy_type == "access_rate_dynamic"){
                        auto key = std::to_string(key_index);
                        if(block_cache->get_cache()->put_access_rate_match(key, value)){
                          block_cache->cache_freq_addition++;
                          snapshot->update_access_rate(key_index);
                        }
                      }
                      server.append_to_rdma_get_response_queue(remote_index, remote_port, ResponseType::OK, value);
                    }
                    else
                    {
                      remote_disk_access.fetch_add(1, std::memory_order::relaxed);
                      snapshot->update_remote_disk_access(expected_key);
                      rdma_node.rdma_key_value_cache->update_local_key(expected_key, key_index, value);
                      LOG_RDMA_DATA("[Read RDMA Callback] Fetching from disk instead key {} != expected {}", key_index, expected_key);
                      fetch_from_disk(false);
                    } });
                }
                else
                {
                  total_rdma_executed.fetch_add(1, std::memory_order::relaxed);
                  read_correct_node(ops_config, rdma_nodes, server_start_index, key_index, read_buffer, &server, remote_index, remote_port);
                  found_in_rdma = true;
                }

                if (!ops_config.RDMA_ASYNC)
                {
                  auto buffer = std::string_view(static_cast<char *>(read_buffer), ops_config.VALUE_SIZE);
                  server.get_response(remote_index, remote_port, ResponseType::OK, buffer);
                }
              }

              if (!found_in_rdma)
              {
                local_disk_access.fetch_add(1, std::memory_order::relaxed);
                snapshot->update_local_disk_access(key_index);
                fetch_from_disk(true);
              }
            }

            snapshot->update_total_access(key_index);
          }
          else if (data.isRdmaSetupRequest())
          {
            auto p = data.getRdmaSetupRequest();
            auto machine_index = p.getMachineIndex();
            auto start_address = p.getStartAddress();
            auto size = p.getSize();
            server.rdma_setup_response(remote_index, ResponseType::OK);
          }
          else if (data.isRdmaSetupResponse())
          {
            auto p = data.getRdmaSetupResponse();
            info("RDMA setup response [reponse_type = {}]",
                 magic_enum::enum_name(p.getResponse()));
          }
          else if (data.isSingletonPutRequest())
          {
            LOG_STATE("[{}-{}:{}] Put response [{}]", machine_index, remote_index, remote_port,
                      kj::str(data).cStr());
            LOG_RDMA_DATA("[Server] Singleton put request");
            auto p = data.getSingletonPutRequest();
            std::string keyStr = p.getKey().cStr(); // Convert capnp::Text::Reader to std::string
            std::string valueStr = p.getValue().cStr();
            LOG_RDMA_DATA("[Server] Singleton put request key = {} value = {} singleton = {} forward_count = {}",
                          keyStr, valueStr, p.getSingleton(), p.getForwardCount());
            block_cache->get_cache()->put_singleton(p.getKey().cStr(), p.getValue().cStr(), p.getSingleton(), p.getForwardCount());
            LOG_RDMA_DATA("[Server] Singleton put request done");
            // server.singleton_put_response(remote_index, ResponseType::OK);
          }
          else if (data.isDeleteRequest())
          {
            auto p = data.getDeleteRequest();
            auto key_ = p.getKey();
            auto key = key_.cStr();
            auto key_index = std::stoi(key);

            block_cache->get_cache()->delete_key(key);
          }
        });
  }
}

int main(int argc, char *argv[])
{
  google::ParseCommandLineFlags(&argc, &argv, true);

  Configuration ops_config = parseConfigFile(FLAGS_dataset_config);

  signal(SIGINT, [](int)
         { g_stop.store(true); });

  // Cache & DB
  auto config_path = fs::path(FLAGS_config);
  std::ifstream ifs(config_path);
  if (!ifs)
  {
    panic("Initializing config from '{}' does not exist", config_path.string());
  }
  json j = json::parse(ifs);
  auto config = j.template get<BlockCacheConfig>();
  printBlockCacheConfig(config);
  int machine_index = FLAGS_machine_index;
  auto machine_config = config.remote_machine_configs[machine_index];
  auto is_server = machine_config.server;

  if (FLAGS_dump_operations)
  {
    generateDatabaseAndOperationSet(ops_config);
    return 0;
  }

  // Kill machnet
  exec("sudo pkill -9 machnet");

  std::shared_ptr<BlockCache<std::string, std::string>> block_cache = nullptr;
  HashMap<uint64_t, RDMA_connect> rdma_nodes;
  if (is_server)
  {
    block_cache =
        std::make_shared<BlockCache<std::string, std::string>>(config);

    snapshot = std::make_shared<Snapshot>(config, ops_config);

    if (config.policy_type == "access_rate_dynamic")
    {
      static std::thread access_rate_thread([&, block_cache]()
                                            {

        std::this_thread::sleep_for(std::chrono::seconds(30));
        CDFType freq;
        auto total_itrs = 0;
        auto time_to_sleep_in_sec = 240;
        auto time_to_sleep = std::chrono::seconds(time_to_sleep_in_sec);
        std::vector<std::tuple<uint64_t, uint64_t, uint64_t>> latencies;
        while (!g_stop)
        {
          // info("block_cache->get_cache()->is_ready() {}", block_cache->get_cache()->is_ready());
          if(block_cache->get_cache()->is_ready()){
            // std::this_thread::sleep_for(std::chrono::seconds(60));
            // std::this_thread::sleep_for(std::chrono::seconds(120));
            // std::this_thread::sleep_for(std::chrono::seconds(180));
            std::this_thread::sleep_for(time_to_sleep);
            auto current_time = std::chrono::high_resolution_clock::now();
            info("Access rate check triggered");
            block_cache->get_cache()->clear_frequency();
            auto now_get_sort = std::chrono::high_resolution_clock::now();
            get_and_sort_freq(block_cache, freq);
            auto now_get_sort_end = std::chrono::high_resolution_clock::now();
            auto elapsed = now_get_sort_end - now_get_sort;
            auto elapsed_seconds = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
            info("get_and_sort_freq took {} microseconds", elapsed_seconds);
            // for (const auto& [key, value] : freq)
            // {
            //   if(key != 0){
            //     info("key {} value {}", key, value);
            //   }
            // }
            // get_best_access_rates(block_cache, freq, cache_ns, disk_ns, rdma_ns);
            latencies.push_back(std::make_tuple(cache_ns, rdma_ns, disk_ns));
            auto now_itr_through_all_the_perf_values_to_find_optimal = std::chrono::high_resolution_clock::now();
            itr_through_all_the_perf_values_to_find_optimal(block_cache,freq, cache_ns, disk_ns, rdma_ns);
            auto now_itr_through_all_the_perf_values_to_find_optimal_end = std::chrono::high_resolution_clock::now();
            auto elapsed_itr_through_all_the_perf_values_to_find_optimal = now_itr_through_all_the_perf_values_to_find_optimal_end - now_itr_through_all_the_perf_values_to_find_optimal;
            auto elapsed_itr_through_all_the_perf_values_to_find_optimal_seconds = std::chrono::duration_cast<std::chrono::microseconds>(elapsed_itr_through_all_the_perf_values_to_find_optimal).count();
            info("itr_through_all_the_perf_values_to_find_optimal took {} microseconds", elapsed_itr_through_all_the_perf_values_to_find_optimal_seconds);
            write_latency_to_file(latencies);
            auto process_end = std::chrono::high_resolution_clock::now();
            auto process_elapsed = process_end - current_time;
            time_to_sleep = std::chrono::seconds(time_to_sleep_in_sec) - std::chrono::duration_cast<std::chrono::seconds>(process_elapsed);
            if(time_to_sleep.count() < 0){
              time_to_sleep = std::chrono::seconds(0);
            }

            // g_stop.store(true);
          } else {
            std::this_thread::sleep_for(std::chrono::seconds(1));
          }
        } });
      access_rate_thread.detach();
    }

    // Load the database and operations
    // load the cache with part of database
    auto start_client_index = 0;
    for (auto i = 0; i < config.remote_machine_configs.size(); i++)
    {
      auto remote_machine_config = config.remote_machine_configs[i];
      if (remote_machine_config.server)
      {
        break;
      }
      start_client_index++;
    }
    auto server_index = FLAGS_machine_index - start_client_index;
    auto start_keys = server_index * (static_cast<float>(ops_config.NUM_KEY_VALUE_PAIRS) / ops_config.NUM_NODES);
    auto end_keys = (server_index + 1) * (static_cast<float>(ops_config.NUM_KEY_VALUE_PAIRS) / ops_config.NUM_NODES);
    key_min = start_keys;
    key_max = end_keys;

    info("[{}] Loading database for server index {} starting at key {} and ending at {}", machine_index, server_index, start_keys, end_keys);
    std::vector<std::string> keys = readKeysFromFile(ops_config.DATASET_FILE);
    if (keys.empty())
    {
      panic("Dataset keys are empty");
    }
    default_value = std::string(ops_config.VALUE_SIZE, 'A');
    auto value = default_value;

    // if (!config.baseline.one_sided_rdma_enabled)
    // {
    //   for (const auto &k : keys)
    //   {
    //     auto key_index = std::stoi(k);
    //     if (key_index >= start_keys && key_index < end_keys)
    //     {
    //       block_cache->get_db()->put(k, value);
    //     }
    //     else
    //     {
    //       block_cache->get_db()->put(k, value);
    //     }
    //   }
    // }

    // Connect to one sided RDMA
    if (config.baseline.one_sided_rdma_enabled)
    {
      int server_start_index;
      for (auto i = 0; i < config.remote_machine_configs.size(); i++)
      {
        if (config.remote_machine_configs[i].server)
        {
          server_start_index = config.remote_machine_configs[i].index;
          break;
        }
      }
      auto result = std::async(std::launch::async, RDMA_Server_Init, RDMA_PORT, 1 * GB_TO_BYTES, FLAGS_machine_index, ops_config);

      sleep(10);

      rdma_nodes = connect_to_servers(config, FLAGS_machine_index, ops_config.VALUE_SIZE, ops_config, block_cache);
      void *local_memory = result.get();

      rdma_nodes[machine_index].local_memory_region = local_memory;

      for (auto &t : rdma_nodes)
      {
        printRDMAConnect(t.second);
      }
      info("print RDMA Connect completed");

      if (config.baseline.one_sided_rdma_enabled && config.baseline.use_cache_indexing)
      {
        info("if (config.baseline.one_sided_rdma_enabled && config.baseline.use_cache_indexing)");
        auto device_name = find_nic_containing(ops_config.infinity_bound_nic);
        auto *context1 = new infinity::core::Context(*device_name, ops_config.infinity_bound_device_port);
        infinity::memory::Buffer *buffer_to_receive1 = new infinity::memory::Buffer(context1, 4096 * sizeof(char));
        context1->postReceiveBuffer(buffer_to_receive1);

        auto *qpf1 = new infinity::queues::QueuePairFactory(context1);

        auto *context2 = new infinity::core::Context(*device_name, ops_config.infinity_bound_device_port);
        infinity::memory::Buffer *buffer_to_receive2 = new infinity::memory::Buffer(context2, 4096 * sizeof(char));
        context2->postReceiveBuffer(buffer_to_receive2);

        auto *qpf2 = new infinity::queues::QueuePairFactory(context2);

        auto start_client_index = 0;
        for (auto i = 0; i < config.remote_machine_configs.size(); i++)
        {
          auto remote_machine_config = config.remote_machine_configs[i];
          if (remote_machine_config.server)
          {
            break;
          }
          start_client_index++;
        }

        auto rdma_key_value_cache = std::make_shared<RDMAKeyValueCache>(config, ops_config, machine_index - start_client_index, context1, qpf1,
                                                                        block_cache->get_rdma_key_value_storage(), block_cache);
        for (auto &[t, node] : rdma_nodes)
        {
          node.rdma_key_value_cache = rdma_key_value_cache;
        }

        bool finished_running_keys = false;
        auto &rdma_node = std::begin(rdma_nodes)->second;
        auto count_expected = 0;
        auto count_finished = 0;
        // std::thread t([&](){
        //   while (!finished_running_keys)
        //   {
        //     rdma_node.rdma_key_value_cache->execute_pending([&](const auto& v)
        //     {
        //       const auto& [kv, _, remote_index, __] = v;
        //       auto key_index = kv->key_index;
        //       auto value = std::string_view((const char*)kv->data, ops_config.VALUE_SIZE);
        //       info("[Execute pending for RDMA] [{}] key {} value {}", remote_index, key_index, value);
        //     }, [&](){
        //     });
        //   }
        // });

        info("adding keys to the blockcache");
        for (const auto &k : keys)
        {
          auto key_index = std::stoi(k);
          if (key_index >= start_keys && key_index < end_keys && config.policy_type == "thread_safe_lru")
          {
            block_cache->put(k, value);
            count_expected++;
          }
          else
          {
            block_cache->get_db()->put(k, value);
          }
        }

        if (0)
        {
          auto count = 0;
          for (const auto &k : keys)
          {
            auto key_index = std::stoi(k);
            if (!(key_index >= start_keys && key_index < end_keys))
            {
              // if (machine_index - 1 != 0)
              {
                info("Request read {}", k);
                rdma_node.rdma_key_value_cache->read(1, key_index);
              }
              if (count > 1000)
              {
                break;
              }
              count++;
            }
          }
        }

        // std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        // while (count_expected >count_finished)
        // {
        //   count_finished = rdma_node.rdma_key_value_cache->get_writes();
        //   std::this_thread::yield();
        // }
        // finished_running_keys = true;
        // t.join();
      }
      else
      {
        info("adding keys to the blockcache in the else part");
        for (const auto &k : keys)
        {
          auto key_index = std::stoi(k);
          if (key_index >= start_keys && key_index < end_keys && config.policy_type == "thread_safe_lru")
          {
            block_cache->put(k, value);
          }
          else
          {
            block_cache->get_db()->put(k, value);
          }
        }
      }

      // Fill in each buffer with value
      std::array<uint8_t, BLKSZ> write_buffer;
      std::fill(write_buffer.begin(), write_buffer.end(), 0);
      std::copy(value.begin(), value.end(), write_buffer.begin());

      // write the value into buffer
      if (!config.baseline.use_cache_indexing)
      {
        info("writing keys");
        for (const auto &k : keys)
        {
          auto key_index = std::stoi(k);
          if (key_index >= start_keys && key_index < end_keys)
          {
            write_correct_node(ops_config, rdma_nodes, server_start_index, key_index, write_buffer);
          }
        }
      }

      // {
      //   auto& node = rdma_nodes[1];
      //   for (auto& [t, node] : rdma_nodes)
      //   {
      //     info("{} {}", t, machine_index);
      //   }
      //   for (auto i = 0; i < 500; i++)
      //   {
      //     node.rdma_key_value_cache->read(0, std::to_string(i));
      //   }
      // }
    }
    info("Running server");
  }
  else
  {
    info("Running client");
  }

  // Launch machnet now
  std::thread machnet_thread(exec_machnet, "cd ../third_party/machnet/ && echo \"y\" | ./machnet.sh 2>&1");
  machnet_thread.detach();

  // Wait for machnet to start
  std::unique_lock lk(machnet_sync.m);
  machnet_sync.cv.wait(lk, []
                       { return machnet_sync.ready; });
  lk.unlock();

  // Connect to machnet
  auto ret = machnet_init();
  assert_with_msg(ret == 0, "machnet_init() failed");

  Operations ops = loadOperationSetFromFile(ops_config.OP_FILE);

  std::vector<std::thread> worker_threads;
  std::vector<std::thread> RDMA_Server_threads;
  std::vector<std::shared_ptr<Client>> clients;
  std::vector<std::shared_ptr<Server>> servers;

  if (is_server)
  {
    static std::thread background_monitoring_thread([&, block_cache]()
                                                    {
      uint64_t last_ops_executed = 0;
      uint64_t last_rdma_executed = 0;
      uint64_t last_disk_executed = 0;
      uint64_t last_cache_reads = 0;
      uint64_t last_cache_hits = 0;
      uint64_t last_cache_misses = 0;
      uint64_t last_remote_disk_access = 0;
      uint64_t last_local_disk_access = 0;
      while (!g_stop)
      {
        auto current_rdma_executed = total_rdma_executed.load(std::memory_order::relaxed);
        auto diff_rdma_executed = current_rdma_executed - last_rdma_executed;
        auto current_ops_executed = total_ops_executed.load(std::memory_order::relaxed);
        auto diff_ops_executed = current_ops_executed - last_ops_executed;
        auto current_disk_executed = total_disk_ops_executed.load(std::memory_order::relaxed);
        auto diff_disk_executed = current_disk_executed - last_disk_executed;
        auto current_remote_disk_access = remote_disk_access.load(std::memory_order::relaxed);
        auto diff_remote_disk_access = current_remote_disk_access - last_remote_disk_access;
        auto current_local_disk_access = local_disk_access.load(std::memory_order::relaxed);
        auto diff_local_disk_access = current_local_disk_access - last_local_disk_access;

        auto cache_info = block_cache->dump_cache_info_as_json();
        uint64_t current_cache_reads{};
        uint64_t current_cache_hits{};
        uint64_t current_cache_misses{};
        cache_info.at("reads").get_to(current_cache_reads);
        cache_info.at("cache_hit").get_to(current_cache_hits);
        cache_info.at("cache_miss").get_to(current_cache_misses);
        auto diff_cache_reads = current_cache_reads - last_cache_reads;
        auto diff_cache_hits = current_cache_hits - last_cache_hits;
        auto diff_cache_misses = current_cache_misses - last_cache_misses;

        info("Ops [{}] +[{}] | RDMA [{}] +[{}] | Disk [{}] +[{}] | C Read [{}] +[{}] | C Hit [{}] +[{}] | C Miss [{}] +[{}] | R Disk [{}] +[{}] | L Disk [{}] +[{}]", 
            current_ops_executed, diff_ops_executed,
            current_rdma_executed, diff_rdma_executed,
            current_disk_executed, diff_disk_executed,
            current_cache_reads, diff_cache_reads,
            current_cache_hits, diff_cache_hits,
            current_cache_misses, diff_cache_misses,
            current_remote_disk_access, diff_remote_disk_access,
            current_local_disk_access, diff_local_disk_access
        );

        last_rdma_executed = current_rdma_executed;
        last_ops_executed = current_ops_executed;
        last_disk_executed = current_disk_executed;
        last_cache_reads = current_cache_reads;
        last_cache_hits = current_cache_hits;
        last_cache_misses = current_cache_misses;
        last_remote_disk_access = current_remote_disk_access;
        last_local_disk_access = current_local_disk_access;

        std::this_thread::sleep_for(std::chrono::seconds(1));
      } });
    background_monitoring_thread.detach();

    for (auto i = 0; i < FLAGS_threads; i++)
    {
      auto server = std::make_shared<Server>(config, ops_config, FLAGS_machine_index, i, block_cache);
      servers.emplace_back(server);
    }
    info("Setup server done");
  }
  else
  {
    static std::thread background_monitoring_thread([&]()
                                                    {
      uint64_t last_ops_executed = 0;
      auto ops_executed_same_time = 0;
      while (!g_stop)
      {
        auto current_ops_executed = total_ops_executed.load(std::memory_order::relaxed);
        auto diff_ops_executed = current_ops_executed - last_ops_executed;
        info("Ops executed [{}] +[{}]", current_ops_executed, diff_ops_executed);
        for (auto i = 0; i < client_thread_ops_executed.size(); i++)
        {
          const auto ops = client_thread_ops_executed[i];
          info("\t[{}] Ops executed [{}]", i, ops);
        }
        if (last_ops_executed == current_ops_executed && last_ops_executed != 0 && current_ops_executed != 0)
        {
          ops_executed_same_time++;
          if (ops_executed_same_time > 10 && current_ops_executed < 100000)
          {
            panic("Ops executed same time for more than 10 seconds... Program must be stuck");
          }
        }
        else
        {
          ops_executed_same_time = 0;
        }
        last_ops_executed = current_ops_executed;
        std::this_thread::sleep_for(std::chrono::seconds(1));
      } });
    background_monitoring_thread.detach();

    client_thread_ops_executed.resize(FLAGS_threads * FLAGS_clients_per_threads);
#ifdef INIT_CLIENTS_IN_PARALLEL
    std::vector<std::future<std::shared_ptr<Client>>> client_futures;
    for (auto i = 0; i < FLAGS_threads; i++)
    {
      for (auto j = 0; j < FLAGS_clients_per_threads; j++)
      {
        client_futures.emplace_back(std::async(std::launch::async, [&config, &ops_config, machine_index, i]
                                               { return std::make_shared<Client>(config, ops_config, FLAGS_machine_index, i); }));
      }
    }
    for (auto &f : client_futures)
    {
      clients.emplace_back(f.get());
    }
#else
    for (auto i = 0; i < FLAGS_threads; i++)
    {
      for (auto j = 0; j < FLAGS_clients_per_threads; j++)
      {
        auto client = std::make_shared<Client>(config, ops_config, FLAGS_machine_index, i);
        clients.emplace_back(client);
      }
    }
#endif
    info("Setup client done {} {}", client_futures.size(), clients.size());
  }

  std::vector<std::thread> rdma_key_value_cache_workers;
  if (is_server)
  {
    auto &rdma_node = std::begin(rdma_nodes)->second;
    if (config.baseline.one_sided_rdma_enabled && config.baseline.use_cache_indexing)
    {
      for (auto i = 0; i < 4; i++)
      {
        std::thread t([&]()
                      {
          while (!g_stop)
          {
            rdma_node.rdma_key_value_cache->execute_pending([&](const auto& v)
            {
              // const auto& [kv, _, remote_index, remote_port] = v;
              // auto key_index = kv->key_index;
              // auto value = std::string_view((const char*)kv->data, ops_config.VALUE_SIZE);
              // info("[Execute pending for RDMA] [{}] key {} value {}", remote_index, key_index, value);
              // auto expected_key = 0;
              // if (key_index == expected_key)
              // {
              //   server.append_to_rdma_get_response_queue(remote_index, remote_port, ResponseType::OK, value);
              // }
              // else
              // {
              //   // TODO: read from disk instead, we need to know expected key
              //   server.append_to_rdma_get_response_queue(remote_index, remote_port, ResponseType::OK, value);
              // }
            }, [](){});
          } });
        rdma_key_value_cache_workers.emplace_back(std::move(t));
      }
    }
  }

  for (auto i = 0; i < FLAGS_threads; i++)
  {
    info("Running {} thread {}", i, i);
    if (is_server)
    {
      auto server = servers[i];
      std::thread t(server_worker, server, config, ops_config, machine_index, i,
                    block_cache, rdma_nodes);
      worker_threads.emplace_back(std::move(t));
    }
    else
    {
      for (auto j = 0; j < FLAGS_clients_per_threads; j++)
      {
        auto client = clients[i * FLAGS_clients_per_threads + j];
        std::thread t(client_worker, client, config, ops_config, FLAGS_machine_index, i, ops, j);
        worker_threads.emplace_back(std::move(t));
      }
    }
  }

  for (auto &t : worker_threads)
  {
    t.join();
  }

  for (auto &t : pollingThread)
  {
    t.join();
  }

  for (auto &t : rdma_key_value_cache_workers)
  {
    t.join();
  }

  if (block_cache)
  {
    block_cache->dump_cache(FLAGS_cache_dump_path);
    auto j = block_cache->dump_cache_info_as_json();
    if (is_server)
    {
      for (auto i = 0; i < FLAGS_threads; i++)
      {
        j["server_stats"].push_back(servers[i]->get_stats());
      }
      j["local_disk_access"] = local_disk_access.load();
      j["remote_disk_access"] = remote_disk_access.load();
      j["total_reads"] = total_ops_executed.load();
    }
    std::ofstream ofs(FLAGS_cache_metrics_path, std::ios::out | std::ios::trunc);
    if (!ofs)
    {
      panic("Unable to open file {}", FLAGS_cache_metrics_path);
    }
    ofs << j.dump(2);
  }

  return 0;
}
