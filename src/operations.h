// operations

#ifndef OPS_H
#define OPS_H

#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <ctime>
#include <random>
#include <cmath>
#include <gflags/gflags.h>
#include <atomic>
#include <nlohmann/json.hpp>

// using namespace gflags;

enum DistributionType {
    RANDOM_DISTRIBUTION,
    PARTITIONED_DISTRIBUTION,
    ZIPFIAN_DISTRIBUTION,
    ZIPFIAN_PARTITIONED_DISTRIBUTION,
    SINGLE_NODE_HOT_KEYS
};


struct Configuration {
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
};

std::ostream& operator<<(std::ostream& os, const Configuration& config);

Configuration parseConfigFile(const std::string& configFile);

void createAndWriteDataset(const std::string& datasetFile, int numKeyValue, int keySize, int valueSize);

std::vector<std::pair<std::string, int>> generateRandomOperationSet(const std::vector<std::string>& keys, Configuration& config, int totalOps, int numNodes);

std::vector<std::pair<std::string, int>> generatePartitionedOperationSet(const std::vector<std::string>& keys, Configuration& config, int totalOps, int numNodes);

std::vector<std::pair<std::string, int>> generateZipfianOperationSet(const std::vector<std::string>& keys, Configuration& config, int totalOps, int numNodes);

std::vector<std::pair<std::string, int>> generateZipfianPartitionedOperationSet(const std::vector<std::string>& keys, Configuration& config, int totalOps, int numNodes);

std::vector<std::pair<std::string, int>> singleNodeHotSetData(const std::vector<std::string>& keys, Configuration& config, int totalOps, int numNodes);

void executeOperations(const std::vector<std::pair<std::string, int>>& operationSet);

void dumpOperationSetToFile(const std::vector<std::pair<std::string, int>>& operationSet);

std::vector<std::string> readKeysFromFile(const std::string& datasetFile);

#endif // OPS_H

