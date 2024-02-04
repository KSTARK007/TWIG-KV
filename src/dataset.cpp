// dataset.cpp

#include "ldc.h"

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

std::vector<std::string> load_database(Configuration &ops_config, std::shared_ptr<BlockDB> &db) {
  createAndWriteDataset(ops_config.DATASET_FILE, ops_config.NUM_KEY_VALUE_PAIRS, ops_config.KEY_SIZE, ops_config.VALUE_SIZE);
  std::vector<std::string> keys = readKeysFromFile(ops_config.DATASET_FILE);

  std::string value;

    for (int j = 0; j < ops_config.VALUE_SIZE; j++) {
        value += static_cast<char>(rand() % 26 + 'A'); // Random uppercase letters
    }

    for (const std::string& key : keys) {
        if (auto err = db->put(key, value); err != DBError::None) {
            panic("Error writing: {}", magic_enum::enum_name(err));
        }
    }
    

    return keys;

}

int issueOps(Configuration &ops_config, std::vector<std::string>& keys){
    std::vector<std::pair<std::string, int>> operationSet;
    switch (ops_config.DISTRIBUTION_TYPE) {
        case RANDOM_DISTRIBUTION:
            operationSet = generateRandomOperationSet(keys, ops_config);
            break;
        case PARTITIONED_DISTRIBUTION:
            operationSet = generatePartitionedOperationSet(keys, ops_config);
            break;
        case ZIPFIAN_DISTRIBUTION:
            operationSet = generateZipfianOperationSet(keys, ops_config);
            break;
        case ZIPFIAN_PARTITIONED_DISTRIBUTION:
            operationSet = generateZipfianPartitionedOperationSet(keys, ops_config);
            break;
        case SINGLE_NODE_HOT_KEYS:
            operationSet = singleNodeHotSetData(keys, ops_config);
            break;
        default:
            std::cerr << "Invalid distribution type selected." << std::endl;
            return 1;
    }
    return 0;
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