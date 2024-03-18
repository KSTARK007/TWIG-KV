#include <stdlib.h>
#include <iostream>
#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <cassert>
#include <thread>
#include <mutex>
#include <chrono>
#include <span>
#include "ldc.h"
// #include "infinity/infinity.h"

// size_t buffer_size = size_t(5) * 1024 * 1024 * 1024;

void read_correct_node(Configuration &ops_config, HashMap<uint64_t, RDMA_connect> &rdma_nodes, int server_start_index, uint64_t key, void *buffer_data, Server *server, int remoteIndex, int remote_port)
{

	uint64_t remote_offset;
	int node;
	node = calculateNodeAndOffset(ops_config, key, server_start_index, remote_offset);
	// info("key {} Node {} server_index {}", key, node, server_start_index);
	// std::cout<< " key " << key << " node "<< node << " server_index " << server_start_index <<  std::endl;
	rdma_reader_thread(rdma_nodes[node + server_start_index], remote_offset, buffer_data, server, remoteIndex, remote_port);
}

void write_correct_node(Configuration &ops_config, HashMap<uint64_t, RDMA_connect> &rdma_nodes, int server_start_index, uint64_t key, std::span<uint8_t> buffer_data)
{

	uint64_t remote_offset;
	int node;
	node = calculateNodeAndOffset(ops_config, key, server_start_index, remote_offset);
	// info("write key {} Node {} server_index {}", key, node, server_start_index);
	// std::cout<< " key " << key << " node "<< node << " server_index " << server_start_index <<  std::endl;
	rdma_writer_thread(rdma_nodes[node + server_start_index], remote_offset, buffer_data);
}

int calculateNodeAndOffset(Configuration &ops_config, uint64_t key, int &server_start_index, uint64_t &offset)
{
	// Adjusting the key to fit within the total key-value pairs and finding its position.
	uint64_t keys_per_node = static_cast<uint64_t>(ops_config.NUM_KEY_VALUE_PAIRS / ops_config.NUM_NODES);
	int nodeIndex;
	if (key > keys_per_node * (ops_config.NUM_NODES))
	{
		nodeIndex = 0;
		offset = keys_per_node + (key - (keys_per_node * (ops_config.NUM_NODES)));
	}
	else
	{
		nodeIndex = static_cast<int>((key - 1) / keys_per_node);
		offset = (key - 1) % keys_per_node;
	}
	return nodeIndex;
}

void rdma_writer_thread(RDMA_connect &node, uint64_t offset, std::span<uint8_t> buffer_data)
{
	// if(node.isLocal){
	// 	auto *localMemRegion = static_cast<uint8_t*>(node.local_memory_region);
	//     std::memcpy(localMemRegion + offset, buffer_data.data(), buffer_data.size());
	//     return;
	// }
	// std::cout<<"writing buffer"<< buffer_data <<std::endl;
	// std::cout << "writing buffer";
	uint64_t key = 1;
	// infinity buffers for sends
	infinity::memory::Buffer *buffer = new infinity::memory::Buffer(node.context, (void *)buffer_data.data(), buffer_data.size());

	// request token
	infinity::requests::RequestToken request_token(node.context);

	// std::cout << "RDMA call";
	node.qp->write(buffer, 0, node.remote_buffer_token, offset, buffer_data.size(),
				   infinity::queues::OperationFlags(), &request_token);
	// std::cout << "RDMA call got back";
	request_token.waitUntilCompleted();

	// if((offset / BLKSZ) % 10000){
	// 	std::cout << "RDMA wrote data to block "<< offset << " key number : " << (offset * 100 / (4096ULL * 2560 * 1024))  << std::endl;
	// }
	delete buffer;
}

void rdma_reader_thread(RDMA_connect &node, uint64_t offset, void *buffer_data, Server *server, int remoteIndex, int remote_port)
{
	if (server->get_ops_config().RDMA_ASYNC)
	{
#ifdef USE_ORIGINAL_ASYNC_RDMA_DATA
		int index = node.circularBuffer->nextAvailableSlot();
		node.qp->read(node.circularBuffer->buffer[index]->buffer, 0, node.remote_buffer_token, offset, BLKSZ,
					infinity::queues::OperationFlags(), node.circularBuffer->buffer[index]->requestToken);
		node.circularBuffer->addAsyncRdmaOp(index, server, offset, remoteIndex);
#else
		AsyncRdmaOp async_rdma_op = node.circularBuffer->get_next_available_async_rdma_op();
		node.qp->read(async_rdma_op.buffer, 0, node.remote_buffer_token, offset, BLKSZ,
					infinity::queues::OperationFlags(), async_rdma_op.request_token);
		async_rdma_op.server = server;
		async_rdma_op.offset = offset;
		async_rdma_op.remoteIndex = remoteIndex;
		async_rdma_op.remote_port = remote_port;
		node.circularBuffer->enqueue_async_rdma_op(async_rdma_op);
#endif
	} else {
		infinity::requests::RequestToken request_token(node.context);
		node.qp->read(node.buffer, 0, node.remote_buffer_token, offset, BLKSZ,
					infinity::queues::OperationFlags(), &request_token);
		request_token.waitUntilCompleted();
		memcpy(buffer_data, node.buffer->getData(), BLKSZ);
	}
}

#ifdef PRINT_RDMA_LATENCY
void rdma_reader_thread(RDMA_connect &node, uint64_t offset, std::span<uint8_t> buffer_data)
{
	auto start_total = std::chrono::high_resolution_clock::now();
	long long nanoseconds;
	// if(node.isLocal){
	// 	auto *localMemRegion = static_cast<uint8_t*>(node.local_memory_region);
	//     std::memcpy(localMemRegion + offset, buffer_data.data(), buffer_data.size());
	// 	std::cout << "node.isLocal" << "offset " << offset<< std::endl;
	//     return;
	// }
	// std::cout<<"reading buffer"<< buffer_data <<std::endl;
	auto start_buffer_creation = std::chrono::high_resolution_clock::now();
	// infinity buffers for sends
	infinity::memory::Buffer *buffers = new infinity::memory::Buffer(node.context, (void *)buffer_data.data(), buffer_data.size());
	auto elapsed_buffer_creation = std::chrono::high_resolution_clock::now() - start_buffer_creation;
	nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed_buffer_creation).count();
	info("Buffer creation time: {}", std::to_string(nanoseconds));

	// request token
	auto start_request_token = std::chrono::high_resolution_clock::now();
	infinity::requests::RequestToken request_token(node.context);
	auto elapsed_request_token = std::chrono::high_resolution_clock::now() - start_request_token;
	nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed_request_token).count();
	info("Request token creation time: {}", std::to_string(nanoseconds));

	auto start_rdma_read = std::chrono::high_resolution_clock::now();
	node.qp->read(buffers, 0, node.remote_buffer_token, offset, BLKSZ,
				  infinity::queues::OperationFlags(), &request_token);
	auto elapsed_rdma_read = std::chrono::high_resolution_clock::now() - start_rdma_read;
	nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed_rdma_read).count();
	info("RDMA read operation time: {}", std::to_string(nanoseconds));

#ifdef RDMA_ASYNC
	auto *op = new AsyncRdmaWriteOp(&request_token, reinterpret_cast<std::array<uint8_t, BLKSZ> *>(buffer), offset);
#endif
#ifndef RDMA_ASYNC
	auto start_wait = std::chrono::high_resolution_clock::now();
	request_token.waitUntilCompleted();
	auto elapsed_wait = std::chrono::high_resolution_clock::now() - start_wait;
	nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed_wait).count();
	info("Wait until completed time: {}", std::to_string(nanoseconds));
#endif

	// std::cout << "getRemoteAddr: " << node.qp->getRemoteAddr()<<std::endl;

	// if((offset / BLKSZ) % 20000){
	// 	std::cout << "data read for offset" << offset << " key " << (offset / BLKSZ) << std::endl;
	// }
	auto delete_buffer = std::chrono::high_resolution_clock::now();
	delete buffers;
	auto elapsed_delete_buffer = std::chrono::high_resolution_clock::now() - delete_buffer;
	nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed_delete_buffer).count();
	info("Deletion of buffer: {}", std::to_string(nanoseconds));

	auto elapsed = std::chrono::high_resolution_clock::now() - start_total;
	nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();
	info("RDMA READ REQUEST TIME {}", std::to_string(nanoseconds));
}
#endif
