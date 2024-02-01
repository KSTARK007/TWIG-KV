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


// Function to simulate send() call to a node
void send(int node, const std::string& key);

// Function to create the dataset and write it to a file
void createAndWriteDataset(const std::string& datasetFile, int numKeyValue, int keySize, int valueSize);

// Function to generate the operation set
std::vector<std::pair<std::string, int>> generateOperationSetAndDumpToFile(const std::vector<std::string>& keys, int totalOps, int numNodes, const std::string& operationSetFile);

// Function to execute the operation set
void executeOperations(const std::vector<std::pair<std::string, int>>& operationSet);

// Function to read keys from the dataset file
std::vector<std::string> readKeysFromFile(const std::string& datasetFile);

#endif // COMMON_H
