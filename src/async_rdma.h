#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <vector>
#include <thread>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include "infinity/infinity.h"
#include "connection.h"

extern std::vector<std::thread> pollingThread;

#ifdef USE_ORIGINAL_ASYNC_RDMA_DATA
// Assuming AsyncRdmaOp has been modified to remove the constructor
struct AsyncRdmaOp
{
    infinity::memory::Buffer *buffer;
    void *data_buffer;
    void *tmp_data;
    infinity::requests::RequestToken *request_token;
    uint64_t offset;
    Server *server;
    int remoteIndex;
    bool isAvailable;
    std::mutex bufferLock;

    // Constructor is now removed and its functionality is handled by AsyncRdmaOpCircularBuffer
    AsyncRdmaOp()
        : request_token(nullptr), buffer(nullptr), data_buffer(nullptr), offset(0), server(nullptr), remoteIndex(0), isAvailable(true)
    {
    }
};

class AsyncRdmaOpCircularBuffer
{
public:
    std::vector<std::unique_ptr<AsyncRdmaOp>> buffer;
    std::atomic<int> head;
    const int size;
    std::atomic<bool> keepPolling{true};
    std::condition_variable cv;
    int value_size;
    std::vector<std::pair<AsyncRdmaOp *, int>> completedOps;

public:
    AsyncRdmaOpCircularBuffer(int bufferSize, int valuesize, infinity::core::Context *context) : size(bufferSize), value_size(valuesize), head(0)
    {
        info("Initing the buffers");
        for (int i = 0; i < size; ++i)
        {
            auto data_buffer = std::unique_ptr<char[]>(new char[BLKSZ]); // Example using unique_ptr for data buffer
            auto tmp_data = std::unique_ptr<char[]>(new char[BLKSZ]);    // Assuming BLKSZ is defined somewhere
            auto op = std::make_unique<AsyncRdmaOp>();                   // Make a new AsyncRdmaOp
            op->data_buffer = data_buffer.release();
            op->tmp_data = tmp_data.release();
            op->buffer = new infinity::memory::Buffer(context, op->data_buffer, BLKSZ); // Adjust as necessary
            op->request_token = new infinity::requests::RequestToken(context);
            op->offset = 0;
            op->server = nullptr;
            op->remoteIndex = 0;
            op->isAvailable = true;
            buffer.emplace_back(std::move(op));

            info("Added buffers for {}", i);
        }
        std::thread localPollingThread(&AsyncRdmaOpCircularBuffer::pollRdmaOperations, this);
        pollingThread.emplace_back(std::move(localPollingThread));
    }

    ~AsyncRdmaOpCircularBuffer()
    {
        keepPolling = false;
        cv.notify_all(); // Notify all waiting threads to ensure a clean exit.

        for (auto &op : buffer)
        {
            delete op->buffer;
            free(op->data_buffer);
        }
    }

    void addAsyncRdmaOp(int index, Server *server, uint64_t offset, int remoteIndex)
    {
        if (index < 0 || index >= size)
        {
            panic("Index out of bounds for addAsyncRdmaOp");
        }
        // info(" index {} to the remote node {}", index, remoteIndex);
        buffer[index]->server = server;
        buffer[index]->remoteIndex = remoteIndex;
        buffer[index]->offset = offset;
        buffer[index]->isAvailable = false;
        // printBlocks();
    }

    int nextAvailableSlot()
    {
        while (true)
        {
            for (int i = 0; i < size; ++i)
            {
                int index = (head.load(std::memory_order_relaxed) + i) % size;
                {
                    std::unique_lock<std::mutex> lock(buffer[index]->bufferLock);
                    if (buffer[index]->isAvailable)
                    {
                        buffer[index]->isAvailable = false;
                        return index;
                    }
                }
            }
        }
    }

    void markSlotAvailable(int index)
    {
        if (index < 0 || index >= size)
        {
            panic("markSlotAvailable index out of bounds");
        }
        buffer[index]->offset = 0;
        buffer[index]->server = nullptr;
        buffer[index]->remoteIndex = 0;
        buffer[index]->isAvailable = true;
        cv.notify_all();
    }

    void dumpStructureToFile(const std::string &filename) const
    {
        std::ofstream file(filename);
        if (!file.is_open())
        {
            std::cerr << "Failed to open file for writing: " << filename << std::endl;
            return;
        }

        for (const auto &op : buffer)
        {
            file << "Buffer Address: " << op->buffer
                 << ", Data Buffer Address: " << static_cast<void *>(op->data_buffer)
                 << ", Request Token: " << op->request_token
                 << ", Offset: " << op->offset
                 << ", Server: " << op->server
                 << ", Remote Index: " << op->remoteIndex
                 << ", Is Available: " << op->isAvailable << std::endl;

            // Assuming you want to print the contents of data_buffer up to a certain length
            if (op->data_buffer != nullptr)
            {
                file << "Data Buffer Contents: ";
                // Example: Print the first 10 bytes of data_buffer
                const char *data = static_cast<const char *>(op->data_buffer);
                for (size_t i = 0; i < 10; ++i)
                {
                    file << std::setw(2) << std::setfill('0') << std::hex << (data[i] & 0xff) << " ";
                }
                file << std::dec << std::endl; // Switch back to decimal for any further numbers
            }
        }

        file.close();
    }

    void printBlocks() const
    {
        for (const auto &op : buffer)
        {
            if (!op->isAvailable)
            {
                std::cout << "Buffer Address: " << static_cast<void *>(op->buffer)
                          << ", Data Buffer Address: " << static_cast<void *>(op->data_buffer)
                          << ", Request Token: " << static_cast<void *>(op->request_token)
                          << ", Offset: " << op->offset
                          << ", Server: " << op->server
                          << ", Remote Index: " << op->remoteIndex
                          << ", Is Available: " << op->isAvailable << std::endl;

                if (op->data_buffer != nullptr)
                {
                    std::cout << "Data Buffer Contents: ";
                    const char *data = static_cast<const char *>(op->data_buffer);
                    for (size_t i = 0; i < 10; ++i)
                    { // Example: Print the first 10 bytes of data_buffer
                        std::cout << std::setw(2) << std::setfill('0') << std::hex << (data[i] & 0xff) << " ";
                    }
                    std::cout << std::dec << std::endl; // Switch back to decimal
                }
            }
        }
    }

    void printAvailability(const std::string &prefix) const
    {
        std::string availability;
        for (const auto &op : buffer)
        {
            availability += op->isAvailable ? '1' : '0';
        }

        if (availability.find('0') != std::string::npos)
        {
            std::cout << prefix << "|" << availability << "|" << std::endl;
        }
    }

    void pollRdmaOperations()
    {
        while (keepPolling && !g_stop)
        {
            for (size_t i = 0; i < buffer.size(); ++i)
            {
                AsyncRdmaOp *op = buffer[i].get();
                {
                    std::unique_lock<std::mutex> lock(op->bufferLock);
                    if (!op->isAvailable && op->request_token != nullptr && op->server)
                    {
                        if (op->request_token->checkIfCompleted())
                        {
                            completedOps.emplace_back(op, i);
                        }
                    }
                }
            }
            // printAvailability("after adding polls");
            // sleep(10);
            // std::this_thread::sleep_for(std::chrono::microseconds(100));
            for (auto &[op, index] : completedOps)
            {
                memcpy(op->tmp_data, op->buffer->getData(), BLKSZ);
                if (op->tmp_data != nullptr)
                {
                    std::string_view value(reinterpret_cast<char *>(op->tmp_data), value_size);
                    op->server->append_to_rdma_get_response_queue(op->remoteIndex, ResponseType::OK, value);
                    // op->server->get_response(op->remoteIndex, ResponseType::OK, value);
                    // info("{}", value);
                    // info("request completed and processed for index {}", index);
                }
                else
                {
                    panic("incorrect information from RDMA, checkIfCompleted() returned a NULL pointer");
                }
                markSlotAvailable(index);
            }
            completedOps.clear();
            // printAvailability("after completing the IOs");
            // std::this_thread::sleep_for(std::chrono::nanoseconds(100));
        }
    }
};

#else

struct AsyncRdmaOp
{
    infinity::memory::Buffer *buffer;
    void *data_buffer;
    infinity::requests::RequestToken *request_token;
    uint64_t offset;
    Server *server;
    int remoteIndex;
    int remote_port;
    bool isAvailable;

    // Constructor is now removed and its functionality is handled by AsyncRdmaOpCircularBuffer
    AsyncRdmaOp()
        : request_token(nullptr), buffer(nullptr), data_buffer(nullptr), offset(0), server(nullptr), remoteIndex(0), isAvailable(true)
    {
    }
};

struct AsyncDiskReadRequest
{
    Server *server;
    int index;
    int port;
    std::string key;
};

class AsyncRdmaOpCircularBuffer
{
public:
    AsyncRdmaOpCircularBuffer(int bufferSize, int valuesize, infinity::core::Context *context) : size(bufferSize), value_size(valuesize)
    {
        info("AsyncRdmaOpCircularBuffer: Initing {} entries", size);
        for (int i = 0; i < size; ++i)
        {
            auto op = AsyncRdmaOp();
            op.data_buffer = new char[BLKSZ];
            op.buffer = new infinity::memory::Buffer(context, op.data_buffer, BLKSZ); // Adjust as necessary
            op.request_token = new infinity::requests::RequestToken(context);
            op.offset = 0;
            op.server = nullptr;
            op.remoteIndex = 0;
            op.isAvailable = true;

            free_queue.enqueue(op);

            free_disk_queue.enqueue(AsyncDiskReadRequest{});
        }
        std::thread localPollingThread(&AsyncRdmaOpCircularBuffer::background_work, this);
        pollingThread.emplace_back(std::move(localPollingThread));
    }

    AsyncRdmaOp get_next_available_async_rdma_op()
    {
        AsyncRdmaOp async_rdma_op;
        while (free_queue.try_dequeue(async_rdma_op))
        {
            return async_rdma_op;
        }
        return async_rdma_op;
    }

    void enqueue_async_rdma_op(AsyncRdmaOp async_rdma_op)
    {
        pending_queue.enqueue(async_rdma_op);
    }

    void enqueue_disk_request(AsyncDiskReadRequest async_disk_request)
    {
        pending_disk_queue.enqueue(async_disk_request);
    }

    AsyncDiskReadRequest get_next_disk_request()
    {
        AsyncDiskReadRequest async_disk_request;
        while (free_disk_queue.try_dequeue(async_disk_request))
        {
            return async_disk_request;
        }
        return async_disk_request;
    }

    void background_work()
    {
        while (keepPolling && !g_stop)
        {
            bool did_work = false;

            AsyncRdmaOp async_rdma_op;
            if (pending_queue.try_dequeue(async_rdma_op))
            {
                did_work = true;

                auto &op = async_rdma_op;
                if (op.request_token->checkIfCompleted())
                {
                    LOG_STATE("[{}] Background thread finished RDMA request for remote index", op.server->get_machine_index(), op.remoteIndex);
                    std::string_view value(reinterpret_cast<char *>(op.buffer->getData()), value_size);
                    op.server->append_to_rdma_get_response_queue(op.remoteIndex, op.remote_port, ResponseType::OK, value);
                    // op.server->get_response(op.remoteIndex, ResponseType::OK, value);
                    op.request_token->reset();
                    free_queue.enqueue(async_rdma_op);
                }
                else
                {
                    // panic("Async rdma should have completed");
                    pending_queue.enqueue(async_rdma_op);
                }
            }

            // AsyncDiskReadRequest async_disk_request;
            // if (pending_disk_queue.try_dequeue(async_disk_request))
            // {
            //     did_work = true;

            //     const auto& [server, index, port, key] = async_disk_request;
            //     auto block_cache = server->get_block_cache();
            //     std::string value;
            //     if (auto result_or_err = block_cache->get_db()->get(key)) {
            //         value = result_or_err.value();
            //     } else {
            //         panic("Failed to get value from db for key {}", key);
            //     }
            //     if (server->get_ops_config().operations_pollute_cache)
            //     {
            //         server->get_block_cache()->get_cache()->put(key, value);
            //     }

            //     server->append_to_rdma_block_cache_request_queue(index, port, ResponseType::OK, value);

            //     free_disk_queue.enqueue(async_disk_request);
            //     info("Background thread finished disk request for index {}", index);
            // }

            if (!did_work)
            {
                std::this_thread::yield();
            }
        }
    }

public:
    std::vector<std::unique_ptr<AsyncRdmaOp>> buffer;
    const int size;
    std::atomic<bool> keepPolling{true};
    int value_size;

    MPMCQueue<AsyncRdmaOp> free_queue;
    MPMCQueue<AsyncRdmaOp> pending_queue;

    MPMCQueue<AsyncDiskReadRequest> free_disk_queue;
    MPMCQueue<AsyncDiskReadRequest> pending_disk_queue;
};

#endif