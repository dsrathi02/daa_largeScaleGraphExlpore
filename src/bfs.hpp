#ifndef BFS_HPP
#define BFS_HPP

#include "graph.hpp"
#include <vector>
#include <string>

struct BFSResult {
    std::vector<int> distance;
    double execution_time_ms;
    long long traversed_edges;
    double mteps; // Million Traversed Edges Per Second
};

// Sequential Breadth-First Search
BFSResult sequential_bfs(const CSRGraph& graph, int start_node);

// OpenMP Level-Synchronous Parallel Breadth-First Search
BFSResult parallel_bfs_omp(const CSRGraph& graph, int start_node, int num_threads = 0);

// Benchmark helper: Warm-up + 5 Repeats + Median Execution Time calculation
BFSResult run_bfs_with_median_timing(const CSRGraph& graph, int start_node, bool is_parallel, int num_threads = 1, int num_runs = 5);

// Correctness verification function
bool verify_bfs_results(const BFSResult& seq_res, const BFSResult& par_res);

// Edge-case unit testing suite
bool run_edge_case_tests();

#endif // BFS_HPP
