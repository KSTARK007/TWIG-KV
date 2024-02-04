#include "operations.h"

int main() {

    srand(time(NULL)); // Seed for random number generation

    createAndWriteDataset(DATASET_FILE, NUM_KEY_VALUE_PAIRS, KEY_SIZE, VALUE_SIZE);

    std::vector<std::string> keys = readKeysFromFile(DATASET_FILE);
    // std::vector<std::pair<std::string, int>> loadCache;
    // loadCache = generatePartitionedOperationSet(keys, NUM_KEY_VALUE_PAIRS, NUM_NODES);
    // executeOperations(loadCache);


    // DistributionType distributionType = RANDOM_DISTRIBUTION;
    // DistributionType distributionType = PARTITIONED_DISTRIBUTION;
    // DistributionType distributionType = ZIPFIAN_DISTRIBUTION;
    // DistributionType distributionType = ZIPFIAN_PARTITIONED_DISTRIBUTION;
    DistributionType distributionType = SINGLE_NODE_HOT_KEYS;

    std::vector<std::pair<std::string, int>> operationSet;
    switch (distributionType) {
        case RANDOM_DISTRIBUTION:
            operationSet = generateRandomOperationSet(keys, TOTAL_OPERATIONS, NUM_NODES);
            break;
        case PARTITIONED_DISTRIBUTION:
            operationSet = generatePartitionedOperationSet(keys, TOTAL_OPERATIONS, NUM_NODES);
            break;
        case ZIPFIAN_DISTRIBUTION:
            operationSet = generateZipfianOperationSet(keys, TOTAL_OPERATIONS, NUM_NODES);
            break;
        case ZIPFIAN_PARTITIONED_DISTRIBUTION:
            operationSet = generateZipfianPartitionedOperationSet(keys, TOTAL_OPERATIONS, NUM_NODES);
            break;
        case SINGLE_NODE_HOT_KEYS:
            operationSet = singleNodeHotSetData(keys, TOTAL_OPERATIONS, NUM_NODES);
            break;
        default:
            std::cerr << "Invalid distribution type selected." << std::endl;
            return 1;
    }

    // Execute the operations based on the operation set
    // executeOperations(operationSet);

    return 0;
}
