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

#ifdef CLIENT_SYNC_WITH_OTHER_CLIENTS
// Total clients ready/done for syncing clients with workloads
std::atomic<uint64_t> total_clients_ready{};
std::atomic<bool> workload_ready = false;

std::atomic<uint64_t> total_clients_done{};
std::atomic<bool> workload_done = false;
#endif

std::vector<uint64_t> client_thread_ops_executed;

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

void execute_operations(Client &client, const std::vector<std::pair<std::string, int>> &operation_set, int client_start_index, BlockCacheConfig config, Configuration &ops_config,
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
  for (int j = 0; j < ops_config.VALUE_SIZE; j++)
  {
    value += static_cast<char>('A');
  }
  long long run_time = 0;
  auto op_start = std::chrono::high_resolution_clock::now();
  do
  {
    auto io_start = std::chrono::high_resolution_clock::now();
    for (const auto &operation : operation_set)
    {
      io_start = std::chrono::high_resolution_clock::now();
      const std::string &key = operation.first;
      int index = operation.second;
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
      timeStamps.push_back(nanoseconds);
      total_ops_executed.fetch_add(1, std::memory_order::relaxed);
      client_thread_ops_executed[client_index]++;

    }
    auto now = std::chrono::high_resolution_clock::now();
    auto op_end = now - op_start;
    run_time = std::chrono::duration_cast<std::chrono::seconds>(op_end).count();
  } while (run_time < ops_config.TOTAL_RUNTIME_IN_SECONDS);
  info("[{}] [{}] Client done executing {}", machine_index, client_index, timeStamps.size());
  dump_per_thread_latency_to_file(timeStamps, client_index_per_thread, machine_index, thread_index);
  LOG_STATE("wrong_values till now {}", wrong_value);

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
                   int machine_index, int thread_index, std::vector<std::pair<std::string, int>> ops,
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
  std::vector<std::pair<std::string, int>> ops_chunk;
  if (ops_config.DISTRIBUTION_TYPE != DistributionType::YCSB) {
    ops_chunk = get_chunk(ops, FLAGS_threads * FLAGS_clients_per_threads, client_index);
  } else {
    std::string file_name = "client_" + std::to_string(machine_index) + "_thread_" + std::to_string(thread_index) + "_clientPerThread_" + std::to_string(client_index_per_thread) + ".txt";
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

  while (!g_stop)
  {
    server.loop(
        [&](auto remote_index, auto remote_port, MachnetFlow &tx_flow, auto &&data)
        {
          // server.get_perf_monitor().report_stats();
          if (data.isPutRequest())
          {
            auto p = data.getPutRequest();
            block_cache->put(p.getKey().cStr(), p.getValue().cStr());

            server.put_response(remote_index, ResponseType::OK);
          }
          else if (data.isGetRequest())
          {
            auto p = data.getGetRequest();

            auto key_ = p.getKey();
            auto key = key_.cStr();
            auto exists_in_cache = block_cache->exists_in_cache(key);
            if (exists_in_cache)
            {
              // Return the correct key in local cache
              server.get_response(remote_index, remote_port, ResponseType::OK,
                                  block_cache->get(key, false, exists_in_cache));
            }
            else
            {
              // Otherwise, if RDMA is renabled, read from the correct node
              bool found_in_rdma = false;

              auto key_index = std::stoi(key);
              auto division_of_key_value_pairs = static_cast<float>(ops_config.NUM_KEY_VALUE_PAIRS) / 3.0;
              auto remote_machine_index_to_rdma = static_cast<int>(
                  static_cast<float>(key_index) / division_of_key_value_pairs);

              auto base_index = machine_index - server_start_index;

              if (config.baseline.one_sided_rdma_enabled)
              {
                if (!config.ingest_block_index)
                {
                  panic("Supports only ingest_block_index being enabled!");
                }

                LOG_STATE("[{}] Reading remote index {}", machine_index, remote_machine_index_to_rdma);
                if (remote_machine_index_to_rdma != base_index)
                {
                  read_correct_node(ops_config, rdma_nodes, server_start_index, key_index, read_buffer, &server, remote_index, remote_port);
                  found_in_rdma = true;
                  if (!ops_config.RDMA_ASYNC)
                  {
                    auto buffer = std::string_view(static_cast<char *>(read_buffer), ops_config.VALUE_SIZE);
                    server.get_response(remote_index, remote_port, ResponseType::OK, buffer);
                  }
                }
              }

              if (!found_in_rdma)
              {
                std::string value;
                if (ops_config.operations_pollute_cache)
                {
                  if (ops_config.DISK_ASYNC) {
                    if (block_cache->get_cache()->exist(key)) {
                      // Cache hit
                      block_cache->increment_cache_hit();
                      value = block_cache->get_cache()->get(key);
                    } else {
                      // Cache miss
                      block_cache->increment_cache_miss();
                    	block_cache->get_db()->get_async(key, [&server, remote_index, remote_port, key, block_cache](auto value) {
                        // Send the response
                        server.append_to_rdma_block_cache_request_queue(remote_index, remote_port, ResponseType::OK, key, value);
                    	});
                    }
                  } else {
                    LOG_STATE("Fetching from cache/disk {} {}", key, value);
                    value = block_cache->get(key);
                  }
                }
                else
                {
                  if (block_cache->get_cache()->exist(key)) {
                    // Cache hit
                    block_cache->increment_cache_hit();
                    value = block_cache->get_cache()->get(key);
                  } else {
                    // Cache miss
                    LOG_STATE("Fetching from disk {} {}", key, value);
                    block_cache->increment_cache_miss();
                    if (ops_config.DISK_ASYNC) {
                    	block_cache->get_db()->get_async(key, [&server, remote_index, remote_port](auto value) {
                        // Send the response
                        server.append_to_rdma_block_cache_request_queue(remote_index, remote_port, ResponseType::OK, {}, value);
                    	});
                    } else {
                      if (auto result_or_err = block_cache->get_db()->get(key)) {
                        value = result_or_err.value();
                      } else {
                        panic("Failed to get value from db for key {}", key);
                      }
                    }
                  }
                }

                if (config.baseline.one_sided_rdma_enabled)
                {
                  panic("One sided rdma should have found the value by now for key {} from {} to my index {}", key, remote_machine_index_to_rdma, base_index);
                }

                if (!ops_config.DISK_ASYNC)
                {
                  server.get_response(remote_index, remote_port, ResponseType::OK, value);
                }
              }
            }
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

    LOG_STATE("[{}] Loading database for server index {} starting at key {} and ending at {}", machine_index, server_index, start_keys, end_keys);
    std::vector<std::string> keys = readKeysFromFile(ops_config.DATASET_FILE);
    if (keys.empty())
    {
      panic("Dataset keys are empty");
    }
    default_value = std::string(ops_config.VALUE_SIZE, 'A');
    auto value = default_value;
    for (const auto &k : keys)
    {
      auto key_index = std::stoi(k);
      if (key_index >= start_keys && key_index < end_keys)
      {
        block_cache->put(k, value);
      }
      else
      {
        block_cache->get_db()->put(k, value);
      }
    }

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

      {
        auto device_name = find_nic_containing(ops_config.infinity_bound_nic);
        auto *context1 = new infinity::core::Context(*device_name, ops_config.infinity_bound_device_port);
        infinity::memory::Buffer *buffer_to_receive1 = new infinity::memory::Buffer(context1, 4096 * sizeof(char));
        context1->postReceiveBuffer(buffer_to_receive1);

        auto *context2 = new infinity::core::Context(*device_name, ops_config.infinity_bound_device_port);
        infinity::memory::Buffer *buffer_to_receive2 = new infinity::memory::Buffer(context2, 4096 * sizeof(char));
        context2->postReceiveBuffer(buffer_to_receive2);

        auto *qpf1 = new infinity::queues::QueuePairFactory(context1);
        auto *qpf2 = new infinity::queues::QueuePairFactory(context2);
        CacheIndexLogs cache_index_logs(config, ops_config, machine_index, context1, qpf1);
      }

      // Fill in each buffer with value
      std::array<uint8_t, BLKSZ> write_buffer;
      std::fill(write_buffer.begin(), write_buffer.end(), 0);
      std::copy(value.begin(), value.end(), write_buffer.begin());

      // write the value into buffer
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

  std::vector<std::pair<std::string, int>> ops = loadOperationSetFromFile(ops_config.OP_FILE);

  std::vector<std::thread> worker_threads;
  std::vector<std::thread> RDMA_Server_threads;
  std::vector<std::shared_ptr<Client>> clients;
  std::vector<std::shared_ptr<Server>> servers;

  if (is_server)
  {
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
      }
    });
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
    }
    std::ofstream ofs(FLAGS_cache_metrics_path, std::ios::out | std::ios::trunc);
    if (!ofs) {
      panic("Unable to open file {}", FLAGS_cache_metrics_path);
    }
    ofs << j.dump(2);
  }

  return 0;
}
