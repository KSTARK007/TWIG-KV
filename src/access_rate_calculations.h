#include "ldc.h"

#include <set>

std::vector<std::pair<uint64_t,std::string>> get_and_sort_freq(std::shared_ptr<BlockCache<std::string, std::string>> cache);