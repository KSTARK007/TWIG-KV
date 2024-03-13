#include <iostream>
#include <queue>
#include <vector>

// Define a struct to hold your data
struct Node {
    uint64_t free_nodes;
    uint64_t index;

    Node(uint64_t free, uint64_t idx) : free_nodes(free), index(idx) {}

    // Comparator for the max heap based on free_nodes
    bool operator<(const Node& other) const {
        return free_nodes < other.free_nodes; // For max heap, this creates a comparison to make top the max
    }
};

class MaxHeap {
private:
    std::priority_queue<Node> pq;

public:
    // Adds a node to the heap
    void add(uint64_t free_nodes, uint64_t index) {
        pq.push(Node(free_nodes, index));
    }

    // Returns the node with the maximum free_nodes and decrements its free_nodes by 1
    uint64_t getNode() {
        if (pq.empty()) {
            throw std::runtime_error("Heap is empty");
        }

        Node maxNode = pq.top();
        pq.pop();

        // Decrement the free_nodes by 1 and add it back if it's not zero
        if (maxNode.free_nodes - 1 > 0) {
            pq.push(Node(maxNode.free_nodes - 1, maxNode.index));
        }

        return maxNode.index;
    }

    bool isEmpty() const {
        return pq.empty();
    }
};
