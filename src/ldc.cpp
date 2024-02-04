#include "ldc.h"

#include <array>
#include <iostream>

static constexpr uint16_t kPort = 31580;

DEFINE_int64(client, 1, "Running as client");
DEFINE_string(config, "", "JSON config");
DEFINE_string(dataset, "", "Path to dataset");
DEFINE_int64(machine_index, 0, "Index of machine");
DEFINE_int64(threads, 1, "Number of threads");

// assert with message
void assert_with_msg(bool cond, const char *msg) {
  if (!cond) {
    printf("%s\n", msg);
    exit(-1);
  }
}

std::atomic<bool> g_stop{false};

// struct Connection
// {
//   Connection(BlockCacheConfig config_, int local_machine_index_, int
//   remote_machine_index_) : config(config_),
//   local_machine_index_(local_machine_index),
//   remote_machine_index(remote_machine_index_)
//   {
//     void *channel = machnet_attach();
//     assert_with_msg(channel != nullptr, "machnet_attach() failed");

//     auto ret = machnet_listen(channel, FLAGS_local.c_str(), port);
//     assert_with_msg(ret == 0, "machnet_listen() failed");
//   }

//   BlockCacheConfig config;
//   int local_machine_index;
//   int remote_machine_index;
// };

template <typename T, typename T2>
using HashMap = ankerl::unordered_dense::map<T, T2>;

struct ConnectionData {
  void *channel;
  MachnetFlow flow;
};

// hash for MachnetFlow
namespace std {
template <> struct hash<MachnetFlow> {
  std::size_t operator()(const MachnetFlow &flow) const {
    return std::hash<int>{}(flow.src_ip) ^ std::hash<int>{}(flow.dst_ip) ^
           std::hash<int>{}(flow.src_port) ^ std::hash<int>{}(flow.dst_port);
  }
};
} // namespace std

struct Connection {
  Connection(BlockCacheConfig config_, int machine_index_, int thread_index_)
      : config(config_), machine_index(machine_index_),
        thread_index(thread_index_) {
    auto my_machine_config = config.remote_machine_configs[machine_index];
    auto ip = my_machine_config.ip;
    current_port = static_cast<int>(my_machine_config.port);
    // Set this thread's port to be an offset to base port
    auto port = use_next_port();

    info("Connection [{}] {}:{}", thread_index, ip.c_str(), port);

    for (auto i = 0; i < config.remote_machine_configs.size(); i++) {
      const auto &remote_machine_config = config.remote_machine_configs[i];
      dst_ip_to_machine_index[ntohl(
          inet_addr(remote_machine_config.ip.c_str()))] = i;
    }
  }

  void connect_to_remote_machine(int remote_index) {
    auto my_machine_config = config.remote_machine_configs[machine_index];
    auto ip = my_machine_config.ip;
    auto port = use_next_port();

    auto remote_machine_config = config.remote_machine_configs[remote_index];

    auto connection_data = ConnectionData{};
    auto &[channel, flow] = connection_data;
    channel = machnet_attach();
    assert_with_msg(channel != nullptr, "machnet_attach() failed");

    auto ret = machnet_listen(channel, ip.c_str(), port);
    assert_with_msg(ret == 0, "machnet_listen() failed");

    printf("Listening on %s:%d\n", ip.c_str(), port);

    // printf("Sending message to %s:%d\n", ip.c_str(), port);
    // std::string msg = "Hello World!";

    ret = machnet_connect(channel, ip.c_str(), remote_machine_config.ip.c_str(),
                          port, &flow);
    assert_with_msg(ret == 0, "machnet_connect() failed");

    machine_index_to_connection[remote_index] = connection_data;
  }

  void listen() {
    auto my_machine_config = config.remote_machine_configs[machine_index];
    auto ip = my_machine_config.ip;
    auto port = my_machine_config.port;

    auto connection_data = ConnectionData{};
    auto &[channel, flow] = connection_data;
    channel = machnet_attach();
    assert_with_msg(channel != nullptr, "machnet_attach() failed");

    auto ret = machnet_listen(channel, ip.c_str(), port);
    assert_with_msg(ret == 0, "machnet_listen() failed");

    info("Listening on {}:{}", ip, port);

    machine_index_to_connection[machine_index] = connection_data;
  }

  void send(int index, std::string_view data) {
    auto &connection_data = machine_index_to_connection[index];
    auto &[channel, flow] = connection_data;
    auto ret = machnet_send(channel, flow, data.data(), data.size());
    if (ret == -1) {
      panic("machnet_send() failed");
    }
  }

  void put(int index, std::string_view key, std::string_view value) {
    ::capnp::MallocMessageBuilder message;
    Packets::Builder packets = message.initRoot<Packets>();
    ::capnp::List<Packet>::Builder packet = packets.initPackets(1);
    Packet::Data::Builder data = packet[0].initData();
    PutRequest::Builder put_request = data.initPutRequest();
    put_request.setKey(std::string(key));
    put_request.setValue(std::string(value));
    auto m = capnp::messageToFlatArray(message);
    auto p = m.asChars();

    debug("PUT [{}]", kj::str(message.getRoot<Packets>()).cStr());

    // ret = machnet_send(channel, flow, p.begin(), p.size());
    send(index, std::string_view(p.begin(), p.end()));
  }

  std::string get(int index, std::string_view key) {
    ::capnp::MallocMessageBuilder message;
    Packets::Builder packets = message.initRoot<Packets>();
    ::capnp::List<Packet>::Builder packet = packets.initPackets(1);
    Packet::Data::Builder data = packet[0].initData();
    GetRequest::Builder get_request = data.initGetRequest();
    get_request.setKey(std::string(key));
    auto m = capnp::messageToFlatArray(message);
    auto p = m.asChars();

    debug("GET [{}]", kj::str(message.getRoot<Packets>()).cStr());

    // ret = machnet_send(channel, flow, p.begin(), p.size());
    send(index, std::string_view(p.begin(), p.end()));

    std::string value;
    poll_receive([&](auto remote_index, MachnetFlow &tx_flow, auto &&data) {
      if (data.isPutRequest()) {
        auto p = data.getPutRequest();
        printf("Received put request: key = %s, value = %s\n",
               p.getKey().cStr(), p.getValue().cStr());
      } else if (data.isPutResponse()) {
        auto p = data.getPutResponse();
      } else if (data.isGetRequest()) {
        auto p = data.getGetRequest();
      } else if (data.isGetResponse()) {
        auto p = data.getGetResponse();
        value = p.getValue().cStr();
      }
    });
    return value;
  }

  void poll_receive(auto &&handler) {
    bool received_data = false;
    while (!received_data) {
      received_data = receive(handler);
      if (g_stop) {
        break;
      }
    }
  }

  bool receive(auto &&handler) {
    std::array<char, 1024> buf;

    auto &connection_data = machine_index_to_connection[machine_index];
    auto &channel = connection_data.channel;
    MachnetFlow rx_flow;
    auto ret = machnet_recv(channel, buf.data(), buf.size(), &rx_flow);
    assert_with_msg(ret >= 0, "machnet_recvmsg() failed");
    if (ret == 0) {
      return false;
    }

    auto *word = reinterpret_cast<capnp::word *>(buf.data());
    auto received_array = kj::ArrayPtr<capnp::word>(word, word + ret);
    capnp::FlatArrayMessageReader message(received_array);
    Packets::Reader packets = message.getRoot<Packets>();
    for (Packet::Reader packet : packets.getPackets()) {
      auto data = packet.getData();
      debug("Received [{}]", kj::str(data).cStr());

      MachnetFlow tx_flow;
      tx_flow.dst_ip = rx_flow.src_ip;
      tx_flow.src_ip = rx_flow.dst_ip;
      tx_flow.dst_port = rx_flow.src_port;
      tx_flow.src_port = rx_flow.dst_port;

      auto remote_index = dst_ip_to_machine_index[rx_flow.dst_ip];
      handler(remote_index, tx_flow, data);
    }
    return true;
  }

  int use_next_port() { return current_port++; }

  // The config
  BlockCacheConfig config;

  // This machine's index corresponding to the one in the config
  int machine_index;

  // Thread index
  int thread_index;

  // Mapping from remote machine index to flow (for sending)
  HashMap<int, ConnectionData> machine_index_to_connection;
  // HashMap<MachnetFlow, int> flow_to_machine_index;
  HashMap<int, int> dst_ip_to_machine_index;

  // Latest port, increments based on each connection to another machine
  int current_port;
};

struct Client : public Connection {
  Client(BlockCacheConfig config, int machine_index, int thread_index)
      : Connection(config, machine_index, thread_index) {
    for (auto i = 0; i < config.remote_machine_configs.size(); i++) {
      if (i != machine_index) {
        connect_to_remote_machine(i);
      }
    }
  }
};

struct ResponseData {
  std::array<char, 1024> buf;
  MachnetFlow rx_flow;
};

struct Server : public Connection {
  Server(BlockCacheConfig config, int machine_index, int thread_index)
      : Connection(config, machine_index, thread_index) {
    listen();
  }

  void put_response(int index, ResponseType response_type) {
    ::capnp::MallocMessageBuilder message;
    Packets::Builder packets = message.initRoot<Packets>();
    ::capnp::List<Packet>::Builder packet = packets.initPackets(1);
    Packet::Data::Builder data = packet[0].initData();
    PutResponse::Builder put_response = data.initPutResponse();
    put_response.setResponse(response_type);
    auto m = capnp::messageToFlatArray(message);
    auto p = m.asChars();

    debug("PUT RESPONSE [{}]", kj::str(message.getRoot<Packets>()).cStr());

    send(index, std::string_view(p.begin(), p.end()));
  }

  void get_response(int index, ResponseType response_type,
                    std::string_view value) {
    ::capnp::MallocMessageBuilder message;
    Packets::Builder packets = message.initRoot<Packets>();
    ::capnp::List<Packet>::Builder packet = packets.initPackets(1);
    Packet::Data::Builder data = packet[0].initData();
    GetResponse::Builder get_response = data.initGetResponse();
    get_response.setResponse(response_type);
    get_response.setValue(std::string(value));
    auto m = capnp::messageToFlatArray(message);
    auto p = m.asChars();

    debug("GET RESPONSE [{}]", kj::str(message.getRoot<Packets>()).cStr());

    send(index, std::string_view(p.begin(), p.end()));
  }
};

void client_worker(BlockCacheConfig config, int machine_index,
                   int thread_index) {
  // auto remote_machine_config = config.remote_machine_configs[0];
  // auto base_port = static_cast<int>(remote_machine_config.port);
  // // Set this thread's port to be an offset to base port
  // auto port = base_port + thread_index;

  // info("Client [{}] {}:{}", thread_index, FLAGS_local, port);

  // void *channel = machnet_attach();
  // assert_with_msg(channel != nullptr, "machnet_attach() failed");

  // auto ret = machnet_listen(channel, FLAGS_local.c_str(), port);
  // assert_with_msg(ret == 0, "machnet_listen() failed");

  // printf("Listening on %s:%d\n", FLAGS_local.c_str(), port);

  // printf("Sending message to %s:%d\n", FLAGS_remote.c_str(), port);
  // MachnetFlow flow;
  // std::string msg = "Hello World!";

  // ret = machnet_connect(channel, FLAGS_local.c_str(), FLAGS_remote.c_str(),
  // port, &flow); assert_with_msg(ret == 0, "machnet_connect() failed");

  // // ret = machnet_send(channel, flow, msg.data(), msg.size());
  // ::capnp::MallocMessageBuilder message;
  // Packets::Builder packets = message.initRoot<Packets>();
  // ::capnp::List<Packet>::Builder packet = packets.initPackets(1);
  // Packet::Data::Builder data = packet[0].initData();
  // PutRequest::Builder put_request = data.initPutRequest();
  // put_request.setKey("key");
  // put_request.setValue("value");
  // auto m = capnp::messageToFlatArray(message);
  // auto p = m.asChars();
  // info("SS {}", p.size());

  // ret = machnet_send(channel, flow, p.begin(), p.size());
  // if (ret == -1)
  //   printf("machnet_send() failed\n");

  Client client(config, machine_index, thread_index);
  client.put(1, "key", "value");
  auto v = client.get(1, "key");
  if (v != "value") {
    panic("Expected value to be 'value' but got '{}'", v);
  }
}

void server_worker(
    BlockCacheConfig config, int machine_index, int thread_index,
    std::shared_ptr<BlockCache<std::string, std::string>> block_cache) {
  // auto remote_machine_config = config.remote_machine_configs[machine_index];
  // auto base_port = static_cast<int>(remote_machine_config.port);
  // // Set this thread's port to be an offset to base port
  // auto port = base_port + thread_index;

  // info("Server [{}] {}:{}", thread_index, FLAGS_local, port);

  // void *channel = machnet_attach();
  // assert_with_msg(channel != nullptr, "machnet_attach() failed");

  // auto ret = machnet_listen(channel, FLAGS_local.c_str(), port);
  // assert_with_msg(ret == 0, "machnet_listen() failed");

  // printf("Listening on %s:%d\n", FLAGS_local.c_str(), port);

  // printf("Waiting for message from client\n");
  // size_t count = 0;

  // while (!g_stop) {
  //   std::array<char, 1024> buf;
  //   MachnetFlow flow;
  //   ret = machnet_recv(channel, buf.data(), buf.size(), &flow);
  //   assert_with_msg(ret >= 0, "machnet_recvmsg() failed");
  //   if (ret == 0) {
  //     usleep(10);
  //     continue;
  //   }

  //   if (ret % sizeof(capnp::word) != 0)
  //   {
  //     panic("Received message is not aligned to word size {} != {}", ret,
  //     sizeof(capnp::word));
  //   }

  //   auto* word = reinterpret_cast<capnp::word*>(buf.data());
  //   auto received_array = kj::ArrayPtr<capnp::word>(word, word + ret);
  //   capnp::FlatArrayMessageReader message(received_array);
  //   Packets::Reader packets = message.getRoot<Packets>();
  //   for (Packet::Reader packet : packets.getPackets())
  //   {
  //     auto data = packet.getData();

  //     if (data.isPutRequest())
  //     {
  //       auto p = data.getPutRequest();

  //       printf("Received put request: key = %s, value = %s\n",
  //       p.getKey().cStr(), p.getValue().cStr());

  //     }
  //     else if (data.isPutResponse())
  //     {
  //       auto p = data.getPutResponse();
  //     }
  //     else if (data.isGetRequest())
  //     {
  //       auto p = data.getGetRequest();
  //     }
  //     else if (data.isGetResponse())
  //     {
  //       auto p = data.getGetResponse();
  //     }
  //   }

  //   info("AA  {} {}", kj::str(message.getRoot<Packets>()).cStr(), 1);

  //   std::string msg(buf.data(), ret);
  //   printf("Received message: %s, count = %zu\n", msg.c_str(), count++);
  // }
  Server server(config, machine_index, thread_index);
  while (!g_stop) {
    server.poll_receive(
        [&](auto remote_index, MachnetFlow &tx_flow, auto &&data) {
          if (data.isPutRequest()) {
            auto p = data.getPutRequest();
            printf("Received put request: key = %s, value = %s\n",
                   p.getKey().cStr(), p.getValue().cStr());
            block_cache->put(p.getKey().cStr(), p.getValue().cStr());

            server.put_response(remote_index, ResponseType::OK);
          } else if (data.isGetRequest()) {
            auto p = data.getGetRequest();
            server.get_response(remote_index, ResponseType::OK,
                                block_cache->get(p.getKey().cStr()));
          }
        });
  }
}

int main(int argc, char *argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);

  signal(SIGINT, [](int) { g_stop.store(true); });

  // Cache & DB
  auto config_path = fs::path(FLAGS_config);
  std::ifstream ifs(config_path);
  if (!ifs) {
    panic("Initializing config from '{}' does not exist", config_path.string());
  }
  json j = json::parse(ifs);
  auto config = j.template get<BlockCacheConfig>();

  std::shared_ptr<BlockCache<std::string, std::string>> block_cache = nullptr;
  if (!FLAGS_client) {
    info("Running server");
    block_cache =
        std::make_shared<BlockCache<std::string, std::string>>(config);
  } else {
    info("Running client");
    auto dataset_path = fs::path(FLAGS_dataset);
    if (fs::exists(dataset_path)) {
      // TODO: read dataset and run workload
    }
  }

  int ret = machnet_init();
  assert_with_msg(ret == 0, "machnet_init() failed");

  std::vector<std::thread> worker_threads;
  for (auto i = 0; i < FLAGS_threads; i++) {
    info("Running {} thread {}", i, i);
    if (!FLAGS_client) {
      std::thread t(server_worker, config, FLAGS_machine_index, i, block_cache);
      worker_threads.emplace_back(std::move(t));
    } else {
      std::thread t(client_worker, config, FLAGS_machine_index, i);
      worker_threads.emplace_back(std::move(t));
    }
  }

  for (auto &t : worker_threads) {
    t.join();
  }

  return 0;
}
