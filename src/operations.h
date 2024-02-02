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

std::ostream& operator<<(std::ostream& os, const Configuration& config) {
    os << "NUM_KEY_VALUE_PAIRS: " << config.NUM_KEY_VALUE_PAIRS << std::endl;
    os << "NUM_NODES: " << config.NUM_NODES << std::endl;
    os << "KEY_SIZE: " << config.KEY_SIZE << std::endl;
    os << "VALUE_SIZE: " << config.VALUE_SIZE << std::endl;
    os << "TOTAL_OPERATIONS: " << config.TOTAL_OPERATIONS << std::endl;
    os << "OP_FILE: " << config.OP_FILE << std::endl;
    os << "DATASET_FILE: " << config.DATASET_FILE << std::endl;
    os << "DISTRIBUTION_TYPE: " << config.DISTRIBUTION_TYPE << std::endl;
    os << "HOT_KEY_PERCENTAGE: " << config.HOT_KEY_PERCENTAGE << std::endl;
    os << "HOT_KEY_ACCESS_PERCENTAGE: " << config.HOT_KEY_ACCESS_PERCENTAGE << std::endl;
    return os;
}

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

