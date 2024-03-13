#pragma once

#include <ext/machnet.h>
#undef PAGE_SIZE // both define PAGE_SIZE

#include "defines.h"

#include "block_cache.h"
#include "operations.h"

// hash for MachnetFlow
namespace std
{
  template <>
  struct hash<MachnetFlow>
  {
    std::size_t operator()(const MachnetFlow &flow) const
    {
      return std::hash<int>{}(flow.src_ip) ^ std::hash<int>{}(flow.dst_ip) ^
             std::hash<int>{}(flow.src_port) ^ std::hash<int>{}(flow.dst_port);
    }
  };
} // namespace std

struct ConnectionData
{
  MachnetFlow flow;
};

struct Connection
{
  Connection(BlockCacheConfig config_, Configuration ops_config_,
             int machine_index_, int thread_index_);

  const BlockCacheConfig &get_block_cache_config() { return config; }
  const Configuration& get_ops_config() { return ops_config; }

  void connect_to_remote_machine(int remote_index);
  void listen();
  void send(int index, std::string_view data);
  void put(int index, std::string_view key, std::string_view value);
  std::string get(int index, std::string_view key);
  void poll_receive(auto &&handler)
  {
    bool received_data = false;
    while (!received_data)
    {
      received_data = receive(handler);
      if (g_stop)
      {
        break;
      }
    }
  }

  void loop(auto &&handler)
  {
    bool received_data = false;
    while (!received_data)
    {
      execute_pending_operations();
      received_data = receive(handler);
      if (g_stop)
      {
        break;
      }
    }
  }

  virtual void execute_pending_operations() {}
  
  bool receive(auto &&handler)
  {
    std::array<char, 4096> buf;

    MachnetFlow rx_flow;

    auto ret = machnet_recv(channel, buf.data(), buf.size(), &rx_flow);
    assert_with_msg(ret >= 0, "machnet_recvmsg() failed");
    if (ret == 0)
    {
      return false;
    }

    // perf_monitor.record_receive_request(ret);

    auto *word = reinterpret_cast<capnp::word *>(buf.data());
    auto received_array = kj::ArrayPtr<capnp::word>(word, word + ret);
    capnp::FlatArrayMessageReader message(received_array);
    Packets::Reader packets = message.getRoot<Packets>();
    for (Packet::Reader packet : packets.getPackets())
    {
      auto data = packet.getData();

      MachnetFlow tx_flow;
      tx_flow.dst_ip = rx_flow.src_ip;
      tx_flow.src_ip = rx_flow.dst_ip;
      tx_flow.dst_port = rx_flow.src_port;
      tx_flow.src_port = rx_flow.dst_port;

      auto remote_index = dst_ip_to_machine_index[rx_flow.src_ip];
      machine_index_to_connection[remote_index].flow = tx_flow;

      LOG_STATE("[{}-{}] Received [{}]", machine_index, remote_index,
                kj::str(data).cStr());

      handler(remote_index, tx_flow, data);
    }
    return true;
  }

  int use_next_port();

  int get_machine_index() const { return machine_index; }

protected:
  // The config
  BlockCacheConfig config;

  // This machine's index corresponding to the one in the config
  int machine_index;

  // Thread index
  int thread_index;

  // opoeration parameters
  Configuration ops_config;

  // Mapping from remote machine index to flow (for sending)
  HashMap<int, ConnectionData> machine_index_to_connection;
  // HashMap<MachnetFlow, int> flow_to_machine_index;
  HashMap<int, int> dst_ip_to_machine_index;

  // Latest port, increments based on each connection to another machine
  int current_port;

  // Machnet channel
  void *channel;
};

struct Client : public Connection
{
  Client(BlockCacheConfig config, Configuration ops_config, int machine_index,
         int thread_index);
};

struct Server : public Connection
{
  Server(BlockCacheConfig config, Configuration ops_config, int machine_index,
         int thread_index);

  void put_response(int index, ResponseType response_type);
  void get_response(int index, ResponseType response_type,
                    std::string_view value);
  void rdma_setup_request(int index, int my_index, uint64_t start_address,
                          uint64_t size);
  void rdma_setup_response(int index, ResponseType response_type);

  void execute_pending_operations() override;
  void append_to_rdma_get_response_queue(int index, ResponseType response_type,
                                         std::string_view value);

public:
  struct RDMAGetResponse
  {
    int index;
    ResponseType response_type;
    std::string value;
  };

private:
  MPMCQueue<RDMAGetResponse> rdma_get_response_queue;
};
