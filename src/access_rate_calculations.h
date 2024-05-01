#include "ldc.h"

#include <set>

std::vector<std::pair<uint64_t,std::string>> get_and_sort_freq(std::shared_ptr<BlockCache<std::string, std::string>> cache);

void get_best_access_rates(std::shared_ptr<BlockCache<std::string, std::string>> cache, std::vector<std::pair<uint64_t,std::string>>& cdf, uint64_t cache_ns_avg, uint64_t disk_ns_avg, uint64_t rdma_ns_avg);