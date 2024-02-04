// dataset.cpp

#include "operations.h"

// Function to read keys from the dataset file
std::vector<std::string> readKeysFromFile(const std::string& datasetFile) {
    std::vector<std::string> keys;

    // Open the dataset file for reading
    std::ifstream file(datasetFile);
    if (!file.is_open()) {
        std::cerr << "Failed to open dataset file: " << datasetFile << std::endl;
        return keys;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Split each line into key and value (assuming space-separated format)
        std::istringstream iss(line);
        std::string key;
        if (iss >> key) {
            keys.push_back(key);
        }
    }

    file.close();
    return keys;
}

// Function to create the dataset and write it to a file
void createAndWriteDataset(const std::string& datasetFile, int numberOfKeys, int keySize, int valueSize) {
    std::ofstream file(datasetFile);
    if (!file.is_open()) {
        std::cerr << "Failed to open dataset file: " << datasetFile << std::endl;
        return;
    }
    std::cout << "writing to the file" << datasetFile << std::endl; 

    std::string value;

    for (int j = 0; j < valueSize; j++) {
        value += static_cast<char>(rand() % 26 + 'A'); // Random uppercase letters
    }

    for (int i = 1; i < numberOfKeys+1; i++) {
        // Generate a sequential key and a random value
        std::string key = std::to_string(i);
        file << key << ' ' << value << '\n'; // Write key and value to the dataset file
    }

    file.close();
}
