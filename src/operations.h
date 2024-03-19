#pragma once

#include <atomic>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <gflags/gflags.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <random>
#include <sstream>
#include <string>
#include <vector>

// using namespace gflags;

enum DistributionType
{
  RANDOM_DISTRIBUTION,
  PARTITIONED_DISTRIBUTION,
  ZIPFIAN_DISTRIBUTION,
  ZIPFIAN_PARTITIONED_DISTRIBUTION,
  SINGLE_NODE_HOT_KEYS,
  SINGLE_NODE_HOT_KEYS_TO_SECOND_NODE_ONLY,
  SINGLE_NODE_HOT_KEYS_TO_SECOND_NODE_RDMA_ONLY,
  SINGLE_NODE_HOT_KEYS_TO_SECOND_NODE_SPLIT,
  YCSB,
};

struct Configuration
{
  int NUM_KEY_VALUE_PAIRS;
  int NUM_NODES;
  int KEY_SIZE;
  int VALUE_SIZE;
  int TOTAL_OPERATIONS;
  std::string OP_FILE;
  std::string DATASET_FILE;
  DistributionType DISTRIBUTION_TYPE;
  double HOT_KEY_PERCENTAGE;
  double HOT_KEY_ACCESS_PERCENTAGE;
  int HOT_THREAD;
  int RDMA_THREAD;
  float TOTAL_RUNTIME_IN_SECONDS;
  bool RDMA_ASYNC;
  bool DISK_ASYNC;
  std::string infinity_bound_nic;
  int infinity_bound_device_port;
  bool operations_pollute_cache;
};

std::ostream &operator<<(std::ostream &os, const Configuration &config);

Configuration parseConfigFile(const std::string &configFile);

void createAndWriteDataset(const std::string &datasetFile, int numKeyValue,
                           int keySize, int valueSize);

std::vector<std::pair<std::string, int>>
generateRandomOperationSet(const std::vector<std::string> &keys,
                           Configuration &config);

std::vector<std::pair<std::string, int>>
generatePartitionedOperationSet(const std::vector<std::string> &keys,
                                Configuration &config);

std::vector<std::pair<std::string, int>>
generateZipfianOperationSet(const std::vector<std::string> &keys,
                            Configuration &config);

std::vector<std::pair<std::string, int>>
generateZipfianPartitionedOperationSet(const std::vector<std::string> &keys,
                                       Configuration &config);

std::vector<std::pair<std::string, int>>
singleNodeHotSetData(const std::vector<std::string> &keys,
                     Configuration &config);

std::vector<std::pair<std::string, int>>
singleNodeHotSetDataToSecondNodeOnly(const std::vector<std::string> &keys,
                                     Configuration &config);

std::vector<std::pair<std::string, int>>
singleNodeHotSetDataToSecondNodeOnlyRDMA(const std::vector<std::string> &keys,
                     Configuration &config);

std::vector<std::pair<std::string, int>>
singleNodeHotSetDataToSecondNodeOnlyRDMA(const std::vector<std::string> &keys,
                     Configuration &config);

std::vector<std::pair<std::string, int>>
singleNodeHotSetDataTo80SecondNode20Other(const std::vector<std::string> &keys,
                                          Configuration &config);

void dumpOperationSetToFile(
    const std::vector<std::pair<std::string, int>> &operationSet);

std::vector<std::string> readKeysFromFile(const std::string &datasetFile);
std::vector<std::pair<std::string, int>> loadOperationSetFromFile(std::string p);
