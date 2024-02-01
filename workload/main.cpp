// main.cpp

#include "common.h"

int main() {

    srand(time(NULL)); // Seed for random number generation

    // Create the dataset and write it to a file
    createAndWriteDataset(DATASET_FILE, NUM_KEY_VALUE_PAIRS, KEY_SIZE, VALUE_SIZE);

    // Read keys from the dataset file
    std::vector<std::string> keys = readKeysFromFile(DATASET_FILE);

    // Generate the operation set
    std::vector<std::pair<std::string, int>> operationSet = generateOperationSetAndDumpToFile(keys, TOTAL_OPERATIONS, NUM_NODES, OP_FILE);

    // Execute the operations based on the operation set
    executeOperations(operationSet);

    return 0;
}
