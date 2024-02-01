#include "ldc.h"

#include <array>
#include <iostream>

static constexpr uint16_t kPort = 31580;

DEFINE_string(local, "", "Local IP address");
DEFINE_string(remote, "", "Remote IP address");
DEFINE_string(config, "", "JSON config");
DEFINE_string(dataset, "", "Path to dataset");
DEFINE_int64(server_index, -1, "Index of server");
DEFINE_int64(threads, -1, "Number of threads");

// assert with message
void assert_with_msg(bool cond, const char *msg) {
  if (!cond) {
    printf("%s\n", msg);
    exit(-1);
  }
}

std::atomic<bool> g_stop{false};

void client_worker(BlockCacheConfig config, int64_t server_index,
                   int thread_index) {
  void *channel = machnet_attach();
  assert_with_msg(channel != nullptr, "machnet_attach() failed");

  auto remote_machine_config = config.remote_machine_configs[server_index];
  auto base_port = remote_machine_config.port;
  // Set this thread's port to be an offset to base port
  auto port = base_port + thread_index;

  ret = machnet_listen(channel, FLAGS_local.c_str(), kPort);
  assert_with_msg(ret == 0, "machnet_listen() failed");

  printf("Listening on %s:%d\n", FLAGS_local.c_str(), kPort);

  printf("Sending message to %s:%d\n", FLAGS_remote.c_str(), kPort);
  MachnetFlow flow;
  std::string msg = "Hello World!";
  ret = machnet_connect(channel, FLAGS_local.c_str(), FLAGS_remote.c_str(),
                        kPort, &flow);
  assert_with_msg(ret == 0, "machnet_connect() failed");

  const int ret = machnet_send(channel, flow, msg.data(), msg.size());
  if (ret == -1)
    printf("machnet_send() failed\n");
}

void server_worker(BlockCacheConfig config, int64_t server_index,
                   int thread_index) {
  void *channel = machnet_attach();
  assert_with_msg(channel != nullptr, "machnet_attach() failed");

  auto remote_machine_config = config.remote_machine_configs[server_index];
  auto base_port = remote_machine_config.port;
  // Set this thread's port to be an offset to base port
  auto port = base_port + thread_index;
  ret = machnet_listen(channel, FLAGS_local.c_str(), kPort);
  assert_with_msg(ret == 0, "machnet_listen() failed");

  printf("Listening on %s:%d\n", FLAGS_local.c_str(), kPort);

  printf("Waiting for message from client\n");
  size_t count = 0;

  while (true) {
    std::array<char, 1024> buf;
    MachnetFlow flow;
    const ssize_t ret = machnet_recv(channel, buf.data(), buf.size(), &flow);
    assert_with_msg(ret >= 0, "machnet_recvmsg() failed");
    if (ret == 0) {
      usleep(10);
      continue;
    }

    std::string msg(buf.data(), ret);
    printf("Received message: %s, count = %zu\n", msg.c_str(), count++);
  }
}

int main(int argc, char *argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);

  signal(SIGINT, [](int) { g_stop.store(true); });

  // Cache & DB
  auto config_path = fs::path(FLAGS_config);
  auto block_cache =
      BlockCache<std::string, std::string>::InitializeFromConfigFile(
          config_path);
  std::shared_ptr<BlockDB> db = block_cache.get_db();
  auto config = block_cache.get_config();

  auto dataset_path = fs::path(FLAGS_dataset);
  if (fs::exists(dataset_path)) {
    // TODO: read dataset and run workload
  }

  if (auto err = db->put("23123", "DATA"); err != DBError::None) {
    panic("Error writing: {}", magic_enum::enum_name(err));
  }

  int ret = machnet_init();
  assert_with_msg(ret == 0, "machnet_init() failed");

  std::vector<std::thread> worker_threads;
  for (auto i = 0; i < FLAGS_threads; i++) {
    if (FLAGS_remote.empty()) {
      std::thread t(server_worker, config, FLAGS_server_index, i);
      worker_threads.emplace_back(std::move(t));
    } else {
      std::thread t(client_worker, config, FLAGS_server_index, i);
      worker_threads.emplace_back(std::move(t));
    }
  }

  for (auto& t : worker_threads)
  {
    t.join();
  }

  return 0;
}
