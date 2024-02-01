#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <cstdlib>
#include <ctime>
#include <random>

// Function to simulate send() call to a node
void send(int node, const std::string& key, const std::string& value) {
    // Your send() implementation here
    std::cout << "Sending key=" << key << " to Node " << node << std::endl;
}

// Function to read and store configuration from a file
bool readConfig(const std::string& configFile, int& NUM_KEY_VALUE_PAIRS, int& NUM_NODES, int& KEY_SIZE, int& VALUE_SIZE) {
    std::ifstream file(configFile);
    if (!file.is_open()) {
        std::cerr << "Failed to open config file: " << configFile << std::endl;
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string param = line.substr(0, pos);
            std::string value = line.substr(pos + 1);

            if (param == "NUM_KEY_VALUE_PAIRS") {
                NUM_KEY_VALUE_PAIRS = std::stoi(value);
            } else if (param == "NUM_NODES") {
                NUM_NODES = std::stoi(value);
            } else if (param == "KEY_SIZE") {
                KEY_SIZE = std::stoi(value);
            } else if (param == "VALUE_SIZE") {
                VALUE_SIZE = std::stoi(value);
            }
        }
    }
    return true;
}

// Function to create the dataset
std::vector<std::pair<std::string, std::string>> createDataset(int NUM_KEY_VALUE_PAIRS, int KEY_SIZE, int VALUE_SIZE) {
    std::vector<std::pair<std::string, std::string>> keyValuePairs;
    std::string value;
    
    for (int j = 0; j < VALUE_SIZE; j++) {
        value += static_cast<char>(rand() % 26 + 'A'); // Random uppercase letters
    }

    for (int i = 0; i < NUM_KEY_VALUE_PAIRS; i++) {
        // Generate a sequential key and a random value
        std::string key = std::to_string(i);
        keyValuePairs.push_back(std::make_pair(key, value));
    }

    return keyValuePairs;
}

// Function to distribute data among nodes
void issueSplitIO(const std::vector<std::pair<std::string, std::string>>& keyValuePairs, int NUM_NODES) {
    int dataSize = keyValuePairs.size();
    int chunkSize = dataSize / NUM_NODES;

    for (int node = 0; node < NUM_NODES; node++) {
        for (int i = node * chunkSize; i < (node + 1) * chunkSize; i++) {
            const std::string& key = keyValuePairs[i].first;
            const std::string& value = keyValuePairs[i].second;
            
            send(node, key, value); // Simulate send() call to the node
        }
    }
}

void issueRandomIO(const std::vector<std::pair<std::string, std::string>>& keyValuePairs, int NUM_NODES) {
    std::vector<int> indices(keyValuePairs.size());
    for (int i = 0; i < indices.size(); i++) {
        indices[i] = i;
    }

    // Shuffle the indices randomly
    std::shuffle(indices.begin(), indices.end(), std::default_random_engine(std::time(nullptr)));

    int dataSize = keyValuePairs.size();
    int chunkSize = dataSize / NUM_NODES;
    
    int index = 0;
    for (int node = 0; node < NUM_NODES; node++) {
        for (int i = 0; i < chunkSize && index < dataSize; i++) {
            const std::string& key = keyValuePairs[indices[index]].first;
            const std::string& value = keyValuePairs[indices[index]].second;
            
            send(node, key, value); // Simulate send() call to the node
            index++;
        }
    }
}

std::vector<std::string> generateOperationSet(const std::vector<std::pair<std::string, std::string>>& keyValuePairs, int NUM_NODES, int TOTAL_OPERATIONS) {
    std::vector<std::string> operationSet;
    std::random_device rd;
    std::mt19937 gen(rd());

    for (int i = 0; i < TOTAL_OPERATIONS; i++) {
        int randomIndex = std::uniform_int_distribution<int>(0, keyValuePairs.size() - 1)(gen);
        int randomNode = std::uniform_int_distribution<int>(0, NUM_NODES - 1)(gen);
        operationSet.push_back("<" + keyValuePairs[randomIndex].first + "," + std::to_string(randomNode) + ">");
    }

    return operationSet;
}

int main() {
    int NUM_KEY_VALUE_PAIRS = 0;
    int NUM_NODES = 0;
    int KEY_SIZE = 0;
    int VALUE_SIZE = 0;

    if (!readConfig("config.txt", NUM_KEY_VALUE_PAIRS, NUM_NODES, KEY_SIZE, VALUE_SIZE)) {
        return 1;
    }

    srand(time(NULL)); // Seed for random number generation

    // Create the dataset
    std::vector<std::pair<std::string, std::string>> keyValuePairs = createDataset(NUM_KEY_VALUE_PAIRS, KEY_SIZE, VALUE_SIZE);

    // loadCache(keyValuePairs, NUM_NODES);
    issueRandomIO(keyValuePairs, NUM_NODES);

    return 0;
}
