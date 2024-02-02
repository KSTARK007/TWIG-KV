// dataset.cpp

#include "operations.h"

using json = nlohmann::json;

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

Configuration parseConfigFile(const std::string& configFile) {
    Configuration config;

    std::ifstream file(configFile);
    if (!file.is_open()) {
        std::cerr << "Error: Failed to open file:" << configFile << std::endl;
        exit(0);
    }

    json jsonData;
    file >> jsonData; // Assuming you have included nlohmann/json.hpp and properly set up the JSON library

    config.NUM_KEY_VALUE_PAIRS = jsonData["NUM_KEY_VALUE_PAIRS"];
    config.NUM_NODES = jsonData["NUM_NODES"];
    config.KEY_SIZE = jsonData["KEY_SIZE"];
    config.VALUE_SIZE = jsonData["VALUE_SIZE"];
    config.TOTAL_OPERATIONS = jsonData["TOTAL_OPERATIONS"];
    config.OP_FILE = jsonData["OP_FILE"];
    config.DATASET_FILE = jsonData["DATASET_FILE"];

    // Map JSON string to enum
    std::string distributionTypeStr = jsonData["DISTRIBUTION_TYPE"];
    if (distributionTypeStr == "RANDOM_DISTRIBUTION") {
        config.DISTRIBUTION_TYPE = RANDOM_DISTRIBUTION;
    } else if (distributionTypeStr == "PARTITIONED_DISTRIBUTION") {
        config.DISTRIBUTION_TYPE = PARTITIONED_DISTRIBUTION;
    } else if (distributionTypeStr == "ZIPFIAN_DISTRIBUTION") {
        config.DISTRIBUTION_TYPE = ZIPFIAN_DISTRIBUTION;
    } else if (distributionTypeStr == "ZIPFIAN_PARTITIONED_DISTRIBUTION") {
        config.DISTRIBUTION_TYPE = ZIPFIAN_PARTITIONED_DISTRIBUTION;
    } else if (distributionTypeStr == "SINGLE_NODE_HOT_KEYS") {
        config.DISTRIBUTION_TYPE = SINGLE_NODE_HOT_KEYS;
    }
    
    config.HOT_KEY_PERCENTAGE = jsonData["HOT_KEY_PERCENTAGE"];
    config.HOT_KEY_ACCESS_PERCENTAGE = jsonData["HOT_KEY_ACCESS_PERCENTAGE"];
    return config;
}

// // Configuration parseConfigFile(const std::string& configFile) {
    
//     Configuration config;

//     std::ifstream file(configFile);
//     if (!file.is_open()) {
//         std::cerr << "Error: Failed to open file:"<< configFile << std::endl;
//         exit(0);
//     }
//     std::string line;
//     while (std::getline(file, line)) {
//         std::istringstream iss(line);
//         std::string key, value;
//         if (std::getline(iss, key, '=')) {
//             if (std::getline(iss, value)) {
//                 // Remove leading and trailing whitespaces from value
//                 value.erase(0, value.find_first_not_of(" \t\n\r\f\v"));
//                 value.erase(value.find_last_not_of(" \t\n\r\f\v") + 1);

//                 if (key == "NUM_KEY_VALUE_PAIRS") {
//                     config.NUM_KEY_VALUE_PAIRS = std::stoi(value);
//                 } else if (key == "NUM_NODES") {
//                     config.NUM_NODES = std::stoi(value);
//                 } else if (key == "KEY_SIZE") {
//                     config.KEY_SIZE = std::stoi(value);
//                 } else if (key == "VALUE_SIZE") {
//                     config.VALUE_SIZE = std::stoi(value);
//                 } else if (key == "TOTAL_OPERATIONS") {
//                     config.TOTAL_OPERATIONS = std::stoi(value);
//                 } else if (key == "OP_FILE") {
//                     config.OP_FILE = value;
//                 } else if (key == "DATASET_FILE") {
//                     config.DATASET_FILE = value;
//                 } else if (key == "DISTRIBUTION_TYPE") {
//                     if (value == "RANDOM_DISTRIBUTION") {
//                         config.DISTRIBUTION_TYPE = RANDOM_DISTRIBUTION;
//                     } else if (value == "PARTITIONED_DISTRIBUTION") {
//                         config.DISTRIBUTION_TYPE = PARTITIONED_DISTRIBUTION;
//                     } else if (value == "ZIPFIAN_DISTRIBUTION") {
//                         config.DISTRIBUTION_TYPE = ZIPFIAN_DISTRIBUTION;
//                     } else if (value == "ZIPFIAN_PARTITIONED_DISTRIBUTION") {
//                         config.DISTRIBUTION_TYPE = ZIPFIAN_PARTITIONED_DISTRIBUTION;
//                     } else if (value == "SINGLE_NODE_HOT_KEYS") {
//                         config.DISTRIBUTION_TYPE = SINGLE_NODE_HOT_KEYS;
//                     }
//                 } else if (key == "HOT_KEY_PERCENTAGE") {
//                     config.HOT_KEY_PERCENTAGE = std::stod(value);
//                 } else if (key == "HOT_KEY_ACCESS_PERCENTAGE") {
//                     config.HOT_KEY_ACCESS_PERCENTAGE = std::stod(value);
//                 }

//             }
//         }
//     }

//     return config;
// }