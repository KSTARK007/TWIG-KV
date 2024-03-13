#include "operations.h"

// Function to generate the operation set
std::vector<std::pair<std::string, int>> generateRandomOperationSet(const std::vector<std::string>& keys, int totalOps, int numNodes) {
    std::vector<std::pair<std::string, int>> operationSet;

    for (int i = 0; i < totalOps; i++) {
        const std::string& key = keys[rand() % keys.size()];
        int randomNode = rand() % numNodes + 1; // Node numbers start from 1
        operationSet.push_back(std::make_pair(key, randomNode));
    }

    dumpOperationSetToFile(operationSet);
    return operationSet;
}

// Function to generate a partitioned operation set
std::vector<std::pair<std::string, int>> generatePartitionedOperationSet(const std::vector<std::string>& keys, int totalOps, int numNodes) {
    std::vector<std::pair<std::string, int>> operationSet;

    int keysPerNode = keys.size() / numNodes; // Number of keys per node

    for (int i = 0; i < totalOps; i++) {
        const std::string& key = keys[i % keys.size()];
        int node = (static_cast<int>(std::floor(static_cast<double>(std::stoi(key)) / keysPerNode))) % numNodes + 1;
        operationSet.push_back(std::make_pair(key, node));
    }
    
    dumpOperationSetToFile(operationSet);
    return operationSet;
}


// Function to generate a Zipfian-distributed operation set
std::vector<std::pair<std::string, int>> generateZipfianOperationSet(const std::vector<std::string>& keys, int totalOps, int numNodes) {
    std::vector<std::pair<std::string, int>> operationSet;
    int numKeys = keys.size();

    // Calculate the number of hot keys and cold keys based on the percentages
    int numHotKeys = static_cast<int>(HOT_KEY_PERCENTAGE * numKeys);
    int numColdKeys = numKeys - numHotKeys;

    // Generate a random order of keys, where hot keys come first and then cold keys
    std::vector<int> keyOrder(numKeys);
    for (int i = 0; i < numKeys; i++) {
        keyOrder[i] = i;
    }
    std::random_shuffle(keyOrder.begin(), keyOrder.end());

    int hotKeyOps = static_cast<int>(HOT_KEY_ACCESS_PERCENTAGE * totalOps);

    for (int i = 0; i < totalOps; i++) {
        int randomIndex;

        // Determine if the access is for a hot key or cold key
        if (i < hotKeyOps) {
            randomIndex = keyOrder[rand() % numHotKeys];
        } else {
            randomIndex = keyOrder[numHotKeys + rand() % numColdKeys];
        }

        const std::string& key = keys[randomIndex];
        int randomNode = rand() % numNodes + 1;
        operationSet.push_back(std::make_pair(key, randomNode));
    }

    dumpOperationSetToFile(operationSet);
    return operationSet;
}

// Function to generate a Zipfian-distributed partitioned operation set
std::vector<std::pair<std::string, int>> generateZipfianPartitionedOperationSet(const std::vector<std::string>& keys, int totalOps, int numNodes) {
    std::vector<std::pair<std::string, int>> operationSet;
    int numKeys = keys.size();

    // Calculate the number of hot keys and cold keys based on the percentages
    int numHotKeys = static_cast<int>(HOT_KEY_PERCENTAGE * numKeys);
    int numColdKeys = numKeys - numHotKeys;
    int keysPerNode = keys.size() / numNodes;

    // Generate a random order of keys, where hot keys come first and then cold keys
    std::vector<int> keyOrder(numKeys);
    for (int i = 0; i < numKeys; i++) {
        keyOrder[i] = i;
    }
    std::random_shuffle(keyOrder.begin(), keyOrder.end());

    int hotKeyOps = static_cast<int>(HOT_KEY_ACCESS_PERCENTAGE * totalOps);

    for (int i = 0; i < totalOps; i++) {
        int randomIndex;

        // Determine if the access is for a hot key or cold key
        if (i < hotKeyOps) {
            randomIndex = keyOrder[rand() % numHotKeys];
        } else {
            randomIndex = keyOrder[numHotKeys + rand() % numColdKeys];
        }
        const std::string& key = keys[randomIndex];
        int node = (static_cast<int>(std::floor(static_cast<double>(std::stoi(key)) / keysPerNode))) % numNodes + 1;
        operationSet.push_back(std::make_pair(key, node));
    }

    dumpOperationSetToFile(operationSet);
    return operationSet;
}

std::vector<std::pair<std::string, int>> singleNodeHotSetData(const std::vector<std::string>& keys, int totalOps, int numNodes) {
    std::vector<std::pair<std::string, int>> operationSet;
    int numKeys = keys.size();

    // Divide the given keyset into 3 parts and select the middle part as the new keyset
    int startIdx = numKeys / 3;
    int endIdx = 2 * numKeys / 3;
    std::vector<std::string> newKeySet(keys.begin() + startIdx, keys.begin() + endIdx);

    // Calculate the number of hot keys and cold keys based on the percentages
    int numHotKeys = static_cast<int>(HOT_KEY_PERCENTAGE * newKeySet.size());
    int numColdKeys = numKeys - numHotKeys;

    // Create a key order where newKeySet comes first, followed by the remaining keys from the original keyset
    std::vector<std::string> keyOrder;
    keyOrder.reserve(numKeys);
    keyOrder.insert(keyOrder.end(), newKeySet.begin(), newKeySet.end());  // New keyset
    for (int i = 0; i < numKeys; i++) {
        if (i < startIdx || i >= endIdx) {
            keyOrder.push_back(keys[i]);  // Old keyset, not in newKeySet
        }
    }

    int hotKeyOps = static_cast<int>(HOT_KEY_ACCESS_PERCENTAGE * totalOps);

    for (int i = 0; i < totalOps; i++) {
        int randomIndex;

        // Determine if the access is for a hot key or cold key
        if (i < hotKeyOps) {
            randomIndex = rand() % (numHotKeys);
        } else {
            randomIndex = numHotKeys + rand() % numColdKeys;
        }
        const std::string& key = keyOrder[randomIndex];

        // std::cout << "rand() \% numHotKeys: " << (rand() % numHotKeys) << "  randomIndex: " << randomIndex << "\n";

        int randomNode = rand() % numNodes + 1;
        operationSet.push_back(std::make_pair(key, randomNode));
    }

    dumpOperationSetToFile(operationSet);
    return operationSet;
}


// Function to execute the operation set
void executeOperations(BlockCacheConfig config, const std::vector<std::pair<std::string, int>>& operationSet) {
    int operationNumber = 1;

    for (const auto& operation : operationSet) {
        const std::string& key = operation.first;
        int randomNode = operation.second;

        std::cout << "Operation " << operationNumber << ": Sending key=" << key << " to Node " << randomNode << std::endl;

        send(randomNode, key);

        operationNumber++;
    }
}

void dumpOperationSetToFile(const std::vector<std::pair<std::string, int>>& operationSet) {
    std::ofstream file("operations.txt");
    if (!file.is_open()) {
        std::cerr << "Failed to open operation set file: " << "operations.txt" << std::endl;
        return;
    }

    for (const auto& operation : operationSet) {
        file << operation.first << ' ' << operation.second << '\n';
    }

    file.close();
}