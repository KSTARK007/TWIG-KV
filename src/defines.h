#pragma once

// Includes
#include "globals.h"

// Third party
#include <capnp/message.h>
#include <capnp/serialize-packed.h>
#include "concurrentqueue.h"
#include "unordered_dense.h"
#include <gflags/gflags.h>
#include <nlohmann/json.hpp>
#include "infinity/infinity.h"
#include <hdr/hdr_histogram.h>
#include <infiniband/verbs.h>

// Generated
#include "packet.capnp.h"

// Depends
#include <arpa/inet.h>
#include <netinet/in.h>

// Stdlib
#include <csignal>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <errno.h>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <future>
#include <cstring>
#include <span>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <iomanip>

// Alias
template <typename T, typename T2>
using HashMap = ankerl::unordered_dense::map<T, T2>;

template <typename T>
using MPMCQueue = moodycamel::ConcurrentQueue<T>;

using json = nlohmann::json;

inline void assert_with_msg(bool cond, const char *msg)
{
    if (!cond)
    {
        printf("%s\n", msg);
        exit(-1);
    }
}
