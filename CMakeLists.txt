cmake_minimum_required(VERSION 3.3)

add_subdirectory(third_party/machnet)
include_directories(SYSTEM third_party/machnet/third_party/glog/src)
include_directories(SYSTEM third_party/machnet/src)

add_subdirectory(third_party/blkcache)
include_directories(SYSTEM third_party/blkcache)
include_directories(SYSTEM third_party/blkcache/RDMA/src)

add_subdirectory(src)


