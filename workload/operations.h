// common.h

#ifndef COMMON_H
#define COMMON_H

#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <ctime>
#include <random>
#include <cmath>

enum DistributionType {
    RANDOM_DISTRIBUTION,
    PARTITIONED_DISTRIBUTION,
    ZIPFIAN_DISTRIBUTION,
    ZIPFIAN_PARTITIONED_DISTRIBUTION,
    SINGLE_NODE_HOT_KEYS
};

void send(int node, const std::string& key);

void createAndWriteDataset(const std::string& datasetFile, int numKeyValue, int keySize, int valueSize);

std::vector<std::pair<std::string, int>> generateRandomOperationSet(const std::vector<std::string>& keys, int totalOps, int numNodes);

std::vector<std::pair<std::string, int>> generatePartitionedOperationSet(const std::vector<std::string>& keys, int totalOps, int numNodes);

std::vector<std::pair<std::string, int>> generateZipfianOperationSet(const std::vector<std::string>& keys, int totalOps, int numNodes);

std::vector<std::pair<std::string, int>> generateZipfianPartitionedOperationSet(const std::vector<std::string>& keys, int totalOps, int numNodes);

std::vector<std::pair<std::string, int>> singleNodeHotSetData(const std::vector<std::string>& keys, int totalOps, int numNodes);

std::vector<std::pair<std::string, int>> singleNodeHotSetDataToSecondNodeOnly(const std::vector<std::string>& keys, int totalOps, int numNodes);

std::vector<std::pair<std::string, int>> singleNodeHotSetDataTo80SecondNode20Other(const std::vector<std::string>& keys, int totalOps, int numNodes);

void executeOperations(BlockCacheConfig config, const std::vector<std::pair<std::string, int>>& operationSet);

void dumpOperationSetToFile(const std::vector<std::pair<std::string, int>>& operationSet);
std::vector<std::pair<std::string, int>> loadOperationSetFromFile();

std::vector<std::string> readKeysFromFile(const std::string& datasetFile);

#endif // COMMON_H
