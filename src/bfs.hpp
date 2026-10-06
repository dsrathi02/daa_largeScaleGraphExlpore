#ifndef BFS_HPP
#define BFS_HPP

#include "graph.hpp"
#include <vector>

struct BFSResult {
    std::vector<int> distance;
    double execution_time_ms;
    long long traversed_edges;
    double mteps; // Million Traversed Edges Per Second
};

// Sequential Breadth-First Search
BFSResult sequential_bfs(const CSRGraph& graph, int start_node);

// Level-Synchronous Parallel Breadth-First Search (Multi-threaded shared memory)
BFSResult parallel_bfs(const CSRGraph& graph, int start_node, int num_threads = 0);

// Correctness verification function
bool verify_bfs_results(const BFSResult& seq_res, const BFSResult& par_res);

#endif // BFS_HPP
