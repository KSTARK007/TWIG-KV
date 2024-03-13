#include "connection.h"

std::atomic<bool> g_stop;

inline std::string flow_to_string(const MachnetFlow &flow)
{
  char src_address[INET_ADDRSTRLEN];
  char dst_address[INET_ADDRSTRLEN];

  auto src_ip = htonl(flow.src_ip);
  auto dst_ip = htonl(flow.dst_ip);
  inet_ntop(AF_INET, &src_ip, src_address, INET_ADDRSTRLEN);
  inet_ntop(AF_INET, &dst_ip, dst_address, INET_ADDRSTRLEN);

  return fmt::format("[{}:{} -> {}:{}]", src_address, flow.src_port,
                     dst_address, flow.dst_port);
}

Connection::Connection(BlockCacheConfig config_, Configuration ops_config_,
                       int machine_index_, int thread_index_)
    : config(config_), machine_index(machine_index_),
      thread_index(thread_index_), ops_config(ops_config_)
{
  auto my_machine_config = config.remote_machine_configs[machine_index];
  auto ip = my_machine_config.ip;
  // Set this thread's port to be an offset to base port
  auto port = my_machine_config.port + thread_index;
  current_port = my_machine_config.port +
                 (thread_index * config.remote_machine_configs.size());

  LOG_STATE("Connection [{}] {}:{}", thread_index, ip.c_str(), port);

  for (auto i = 0; i < config.remote_machine_configs.size(); i++)
  {
    const auto &remote_machine_config = config.remote_machine_configs[i];
    dst_ip_to_machine_index[ntohl(
        inet_addr(remote_machine_config.ip.c_str()))] = i;
  }

  channel = machnet_attach();
  assert_with_msg(channel != nullptr, "machnet_attach() failed");
}

void Connection::connect_to_remote_machine(int remote_index)
{
  auto my_machine_config = config.remote_machine_configs[machine_index];
  auto ip = my_machine_config.ip;

  // Bind to any next available port
  // auto port = use_next_port();

  auto remote_machine_config = config.remote_machine_configs[remote_index];
  auto remote_port = remote_machine_config.port + thread_index;

  auto connection_data = ConnectionData{};
  auto &[flow] = connection_data;

  // LOG_STATE("[{}-{}] Listening on [{}:{}]", machine_index, remote_index, ip,
  //       port);

  // auto ret = machnet_listen(channel, ip.c_str(), port);
  // assert_with_msg(ret == 0, "machnet_listen() failed");
  int ret = 0;
  auto port = 0;

  constexpr auto MACHNET_CONNECT_RETRIES = 100;
  for (auto i = 0; i < MACHNET_CONNECT_RETRIES; i++)
  {
    info("[{}-{}] {} Connecting from [{}:{}] to [{}:{}]", machine_index,
              remote_index, flow_to_string(flow), ip, port,
              remote_machine_config.ip, remote_port);

    ret = machnet_connect(channel, ip.c_str(), remote_machine_config.ip.c_str(),
                          remote_port, &flow);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    if (ret == 0)
    {
      break;
    }
  }
  assert_with_msg(ret == 0, "machnet_connect() failed");

  LOG_STATE("[{}-{}] {} Connected from [{}:{}] to [{}:{}]", machine_index,
            remote_index, flow_to_string(flow), ip, port,
            remote_machine_config.ip, remote_port);

  machine_index_to_connection[remote_index] = connection_data;

  // TODO: this is for a hack with client, need unique for each connection
  // (this only works with 1 server conn)
  // machine_index_to_connection[machine_index] = connection_data;
}

void Connection::listen()
{
  auto my_machine_config = config.remote_machine_configs[machine_index];
  auto ip = my_machine_config.ip;
  auto port = my_machine_config.port + thread_index;

  auto connection_data = ConnectionData{};
  auto &[flow] = connection_data;

  LOG_STATE("[{}] {} Attempting to listen on [{}:{}]", machine_index,
            flow_to_string(flow), ip, port);

  auto ret = machnet_listen(channel, ip.c_str(), port);
  assert_with_msg(ret == 0, "machnet_listen() failed");

  LOG_STATE("[{}] {} Listening on [{}:{}]", machine_index, flow_to_string(flow),
            ip, port);

  machine_index_to_connection[machine_index] = connection_data;
}

void Connection::send(int index, std::string_view data)
{
  auto &connection_data = machine_index_to_connection[index];
  auto &[flow] = connection_data;
  LOG_STATE("[{}-{}] {} Sending size {}", machine_index, index,
            flow_to_string(flow), data.size());
  auto ret = machnet_send(channel, flow, data.data(), data.size());
  LOG_STATE("[{}-{}] {} Sent size {}", machine_index, index, flow_to_string(flow),
            data.size());
  if (ret == -1)
  {
    panic("machnet_send() failed");
  }
}

void Connection::put(int index, std::string_view key, std::string_view value)
{
  ::capnp::MallocMessageBuilder message;
  Packets::Builder packets = message.initRoot<Packets>();
  ::capnp::List<Packet>::Builder packet = packets.initPackets(1);
  Packet::Data::Builder data = packet[0].initData();
  PutRequest::Builder put_request = data.initPutRequest();
  put_request.setKey(std::string(key));
  put_request.setValue(std::string(value));
  auto m = capnp::messageToFlatArray(message);
  auto p = m.asChars();

  LOG_STATE("[{}-{}] Put request [{}]", machine_index, index,
            kj::str(message.getRoot<Packets>()).cStr());

  send(index, std::string_view(p.begin(), p.end()));

  poll_receive([&](auto remote_index, MachnetFlow &tx_flow, auto &&data)
               {
    if (data.isPutRequest()) {
      auto p = data.getPutRequest();
    } else if (data.isPutResponse()) {
      auto p = data.getPutResponse();
      LOG_STATE("[{}-{}] Put response", machine_index, index);
    } else if (data.isGetRequest()) {
      auto p = data.getGetRequest();
    } else if (data.isGetResponse()) {
      auto p = data.getGetResponse();
    } });
}

std::string Connection::get(int index, std::string_view key)
{
  ::capnp::MallocMessageBuilder message;
  Packets::Builder packets = message.initRoot<Packets>();
  ::capnp::List<Packet>::Builder packet = packets.initPackets(1);
  Packet::Data::Builder data = packet[0].initData();
  GetRequest::Builder get_request = data.initGetRequest();
  get_request.setKey(std::string(key));
  auto m = capnp::messageToFlatArray(message);
  auto p = m.asChars();

  LOG_STATE("[{}-{}] Get request [{}]", machine_index, index,
            kj::str(message.getRoot<Packets>()).cStr());

  send(index, std::string_view(p.begin(), p.end()));

  std::string value;
  poll_receive([&](auto remote_index, MachnetFlow &tx_flow, auto &&data)
               {
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
      LOG_STATE("[{}-{}] Get response [key = {}, value = {}]", machine_index,
            index, key, value);
    } });
  return value;
}

int Connection::use_next_port()
{
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0)
  {
    panic("could not open socket (%d) %s", errno, strerror(errno));
  }

  struct sockaddr_in serv_addr;
  bzero((char *)&serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = 0;
  if (bind(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
  {
    panic("could not bind to process (%d) %s", errno, strerror(errno));
  }

  socklen_t len = sizeof(serv_addr);
  if (getsockname(sock, (struct sockaddr *)&serv_addr, &len) == -1)
  {
    panic("could not get socket name (%d) %s", errno, strerror(errno));
  }

  if (close(sock) < 0)
  {
    panic("could not close socket (%d) %s", errno, strerror(errno));
  }
  auto port = ntohs(serv_addr.sin_port);

  LOG_STATE("FREE PORT {}", port);
  return port;
}

Client::Client(BlockCacheConfig config, Configuration ops_config, int machine_index,
               int thread_index)
    : Connection(config, ops_config, machine_index, thread_index)
{
  for (auto i = 0; i < config.remote_machine_configs.size(); i++)
  {
    if (i != machine_index && config.remote_machine_configs[i].server)
    {
      connect_to_remote_machine(i);
    }
  }
}

Server::Server(BlockCacheConfig config, Configuration ops_config, int machine_index,
               int thread_index)
    : Connection(config, ops_config, machine_index, thread_index)
{
  listen();
}

void Server::put_response(int index, ResponseType response_type)
{
  ::capnp::MallocMessageBuilder message;
  Packets::Builder packets = message.initRoot<Packets>();
  ::capnp::List<Packet>::Builder packet = packets.initPackets(1);
  Packet::Data::Builder data = packet[0].initData();
  PutResponse::Builder put_response = data.initPutResponse();
  put_response.setResponse(response_type);
  auto m = capnp::messageToFlatArray(message);
  auto p = m.asChars();

  LOG_STATE("[{}-{}] Put response [{}]", machine_index, index,
            kj::str(message.getRoot<Packets>()).cStr());

  send(index, std::string_view(p.begin(), p.end()));

  LOG_STATE("[{}-{}] Put response sent [{}]", machine_index, index,
            kj::str(message.getRoot<Packets>()).cStr());
}

void Server::get_response(int index, ResponseType response_type,
                          std::string_view value)
{
  ::capnp::MallocMessageBuilder message;
  Packets::Builder packets = message.initRoot<Packets>();
  ::capnp::List<Packet>::Builder packet = packets.initPackets(1);
  Packet::Data::Builder data = packet[0].initData();
  GetResponse::Builder get_response = data.initGetResponse();
  get_response.setResponse(response_type);
  get_response.setValue(std::string(value));
  auto m = capnp::messageToFlatArray(message);
  auto p = m.asChars();

  LOG_STATE("[{}-{}] Get response [{}]", machine_index, index,
            kj::str(message.getRoot<Packets>()).cStr());

  send(index, std::string_view(p.begin(), p.end()));

  LOG_STATE("[{}-{}] Get response sent [{}]", machine_index, index,
            kj::str(message.getRoot<Packets>()).cStr());
}

void Server::rdma_setup_request(int index, int my_index, uint64_t start_address,
                                uint64_t size)
{
  panic("Deprecated");
  ::capnp::MallocMessageBuilder message;
  Packets::Builder packets = message.initRoot<Packets>();
  ::capnp::List<Packet>::Builder packet = packets.initPackets(1);
  Packet::Data::Builder data = packet[0].initData();
  RdmaSetupRequest::Builder request = data.initRdmaSetupRequest();
  request.setMachineIndex(my_index);
  request.setStartAddress(start_address);
  request.setSize(size);
  auto m = capnp::messageToFlatArray(message);
  auto p = m.asChars();

  LOG_STATE("[{}-{}] RDMA setup request [{}]", machine_index, index,
            kj::str(message.getRoot<Packets>()).cStr());

  send(index, std::string_view(p.begin(), p.end()));
}

void Server::rdma_setup_response(int index, ResponseType response_type)
{
  panic("Deprecated");
  ::capnp::MallocMessageBuilder message;
  Packets::Builder packets = message.initRoot<Packets>();
  ::capnp::List<Packet>::Builder packet = packets.initPackets(1);
  Packet::Data::Builder data = packet[0].initData();
  RdmaSetupResponse::Builder response = data.initRdmaSetupResponse();
  response.setResponse(response_type);
  auto m = capnp::messageToFlatArray(message);
  auto p = m.asChars();

  LOG_STATE("[{}-{}] RDMA setup response [{}]", machine_index, index,
            kj::str(message.getRoot<Packets>()).cStr());

  send(index, std::string_view(p.begin(), p.end()));
}

void Server::execute_pending_operations()
{
  Connection::execute_pending_operations();
  RDMAGetResponse response;
  while (rdma_get_response_queue.try_dequeue(response))
  {
    auto [index, response_type, value] = response;
    if (response_type != ResponseType::OK)
    {
      panic("RDMA get failed");
    }
    LOG_STATE("[{}-{}] Execute pending operation [{}]", machine_index, index, value);
    get_response(index, response_type, value);
  }
}

void Server::append_to_rdma_get_response_queue(int index, ResponseType response_type,
                                               std::string_view value)
{
  auto response = Server::RDMAGetResponse{index, response_type, std::string(value)};
  rdma_get_response_queue.enqueue(response);
}