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
    bool all_runs_verified; // True if 100% of repeated runs matched sequential output
};

// Sequential Breadth-First Search
BFSResult sequential_bfs(const CSRGraph& graph, int start_node);

// OpenMP Level-Synchronous Parallel Breadth-First Search
BFSResult parallel_bfs_omp(const CSRGraph& graph, int start_node, int num_threads = 0);

// Benchmark helper: Warm-up + N Repeats -> Verifies ALL parallel runs against sequential baseline, returns median timing
BFSResult run_bfs_with_median_timing(const CSRGraph& graph, int start_node, bool is_parallel, int num_threads = 1, int num_runs = 5);

// Correctness verification function (element-wise distance match)
bool verify_bfs_results(const BFSResult& seq_res, const BFSResult& par_res);

// Comprehensive unit testing suite including edge cases & multi-threaded stress tests
bool run_edge_case_tests();

#endif // BFS_HPP
