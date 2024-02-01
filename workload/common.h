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
    ZIPFIAN_PARTITIONED_DISTRIBUTION
};

// Function to simulate send() call to a node
void send(int node, const std::string& key);

// Function to create the dataset and write it to a file
void createAndWriteDataset(const std::string& datasetFile, int numKeyValue, int keySize, int valueSize);

// Function to generate a random operation set
std::vector<std::pair<std::string, int>> generateRandomOperationSet(const std::vector<std::string>& keys, int totalOps, int numNodes);

// Function to generate a partitioned operation set
std::vector<std::pair<std::string, int>> generatePartitionedOperationSet(const std::vector<std::string>& keys, int totalOps, int numNodes);

// Function to generate a Zipfian-distributed operation set
std::vector<std::pair<std::string, int>> generateZipfianOperationSet(const std::vector<std::string>& keys, int totalOps, int numNodes);

// Function to generate a Zipfian-distributed partitioned operation set
std::vector<std::pair<std::string, int>> generateZipfianPartitionedOperationSet(const std::vector<std::string>& keys, int totalOps, int numNodes);

// Function to execute the operation set
void executeOperations(const std::vector<std::pair<std::string, int>>& operationSet);

// Function to read keys from the dataset file
std::vector<std::string> readKeysFromFile(const std::string& datasetFile);

#endif // COMMON_H
