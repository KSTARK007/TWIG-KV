// operations.cpp

#include "common.h"

// Function to simulate send() call to a node
void send(int node, const std::string& key) {
    // Your send() implementation here
    std::cout << "Sending key=" << key << " to Node " << node << std::endl;
}

// Function to generate the operation set
std::vector<std::pair<std::string, int>> generateOperationSetAndDumpToFile(const std::vector<std::string>& keys, int totalOps, int numNodes, const std::string& operationSetFile) {
    std::vector<std::pair<std::string, int>> operationSet;

    std::ofstream file(operationSetFile); // Open a file for writing the operation set
    if (!file.is_open()) {
        std::cerr << "Failed to open operation set file: " << operationSetFile << std::endl;
        return operationSet;
    }

    for (int i = 0; i < totalOps; i++) {
        int randomIndex = rand() % keys.size();
        const std::string& key = keys[randomIndex];
        int randomNode = rand() % numNodes;
        operationSet.push_back(std::make_pair(key, randomNode));

        // Write the operation to the operation set file
        file << key << ' ' << randomNode << '\n';
    }

    file.close(); // Close the operation set file
    return operationSet;
}

// Function to execute the operation set
void executeOperations(const std::vector<std::pair<std::string, int>>& operationSet) {

     int operationNumber = 1;

    for (const auto& operation : operationSet) {
        const std::string& key = operation.first;
        int randomNode = operation.second;

        std::cout << "Operation " << operationNumber << ": Sending key=" << key << " to Node " << randomNode << std::endl;

        send(randomNode, key);

        operationNumber++;
    }
}
