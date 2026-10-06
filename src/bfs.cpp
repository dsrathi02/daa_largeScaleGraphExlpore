#include "bfs.hpp"
#include <queue>
#include <chrono>
#include <iostream>
#include <algorithm>
#include <vector>
#include <omp.h>

BFSResult sequential_bfs(const CSRGraph& graph, int start_node) {
    BFSResult result;
    int V = graph.num_vertices;
    result.distance.assign(V, -1);

    if (V == 0 || start_node < 0 || start_node >= V) {
        result.execution_time_ms = 0.0;
        result.traversed_edges = 0;
        result.mteps = 0.0;
        return result;
    }

    std::queue<int> q;

    auto start_time = std::chrono::high_resolution_clock::now();

    result.distance[start_node] = 0;
    q.push(start_node);

    long long traversed_edges = 0;

    while (!q.empty()) {
        int u = q.front();
        q.pop();
        int d = result.distance[u];

        long long start_idx = graph.offsets[u];
        long long end_idx = graph.offsets[u + 1];
        traversed_edges += (end_idx - start_idx);

        for (long long i = start_idx; i < end_idx; ++i) {
            int v = graph.edges[i];
            if (result.distance[v] == -1) {
                result.distance[v] = d + 1;
                q.push(v);
            }
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::micro> duration = end_time - start_time;

    result.execution_time_ms = duration.count() / 1000.0;
    result.traversed_edges = traversed_edges;
    double seconds = result.execution_time_ms / 1000.0;
    result.mteps = (seconds > 0) ? (traversed_edges / 1e6) / seconds : 0.0;

    return result;
}

// OpenMP Level-Synchronous Parallel BFS
BFSResult parallel_bfs_omp(const CSRGraph& graph, int start_node, int num_threads) {
    if (num_threads > 0) {
        omp_set_num_threads(num_threads);
    }

    BFSResult result;
    int V = graph.num_vertices;
    result.distance.assign(V, -1);

    if (V == 0 || start_node < 0 || start_node >= V) {
        result.execution_time_ms = 0.0;
        result.traversed_edges = 0;
        result.mteps = 0.0;
        return result;
    }

    std::vector<int> frontier;
    std::vector<int> next_frontier;

    auto start_time = std::chrono::high_resolution_clock::now();

    result.distance[start_node] = 0;
    frontier.push_back(start_node);

    long long total_traversed_edges = 0;
    int current_level = 0;

    while (!frontier.empty()) {
        next_frontier.clear();
        long long level_edges = 0;

        #pragma omp parallel
        {
            std::vector<int> thread_local_frontier;

            #pragma omp for schedule(dynamic, 512) reduction(+:level_edges)
            for (size_t i = 0; i < frontier.size(); ++i) {
                int u = frontier[i];
                long long start_idx = graph.offsets[u];
                long long end_idx = graph.offsets[u + 1];
                level_edges += (end_idx - start_idx);

                for (long long j = start_idx; j < end_idx; ++j) {
                    int v = graph.edges[j];
                    if (result.distance[v] == -1) {
                        // Atomic CAS claim to guarantee single ownership per vertex v
                        if (__sync_bool_compare_and_swap(&result.distance[v], -1, current_level + 1)) {
                            thread_local_frontier.push_back(v);
                        }
                    }
                }
            }

            // Merge thread-local frontier into global next_frontier safely
            if (!thread_local_frontier.empty()) {
                #pragma omp critical
                {
                    next_frontier.insert(next_frontier.end(), thread_local_frontier.begin(), thread_local_frontier.end());
                }
            }
        }

        total_traversed_edges += level_edges;
        frontier.swap(next_frontier);
        current_level++;
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::micro> duration = end_time - start_time;

    result.execution_time_ms = duration.count() / 1000.0;
    result.traversed_edges = total_traversed_edges;
    double seconds = result.execution_time_ms / 1000.0;
    result.mteps = (seconds > 0) ? (total_traversed_edges / 1e6) / seconds : 0.0;

    return result;
}

// Rigorous Benchmark Helper: 1 Warm-up Run + 5 Repeated Runs -> Median Execution Time
BFSResult run_bfs_with_median_timing(const CSRGraph& graph, int start_node, bool is_parallel, int num_threads, int num_runs) {
    // Warm-up run (not recorded)
    if (is_parallel) {
        parallel_bfs_omp(graph, start_node, num_threads);
    } else {
        sequential_bfs(graph, start_node);
    }

    std::vector<BFSResult> results;
    std::vector<double> times;

    for (int i = 0; i < num_runs; ++i) {
        BFSResult res = is_parallel ? parallel_bfs_omp(graph, start_node, num_threads) : sequential_bfs(graph, start_node);
        results.push_back(res);
        times.push_back(res.execution_time_ms);
    }

    // Sort to pick median
    std::sort(times.begin(), times.end());
    double median_time = times[num_runs / 2];

    // Find result corresponding to median time
    for (const auto& r : results) {
        if (r.execution_time_ms == median_time) {
            return r;
        }
    }

    results[0].execution_time_ms = median_time;
    return results[0];
}

bool verify_bfs_results(const BFSResult& seq_res, const BFSResult& par_res) {
    if (seq_res.distance.size() != par_res.distance.size()) {
        std::cerr << "Mismatch in distance vector sizes!" << std::endl;
        return false;
    }

    for (size_t i = 0; i < seq_res.distance.size(); ++i) {
        if (seq_res.distance[i] != par_res.distance[i]) {
            std::cerr << "Mismatch at vertex " << i 
                      << ": Sequential dist = " << seq_res.distance[i] 
                      << ", Parallel dist = " << par_res.distance[i] << std::endl;
            return false;
        }
    }
    return true;
}

// Edge-Case Testing Suite
bool run_edge_case_tests() {
    std::cout << "\n========================================================\n";
    std::cout << "               RUNNING EDGE-CASE UNIT TESTS             \n";
    std::cout << "========================================================\n";

    bool all_passed = true;

    // Test 1: Single Vertex Graph (V=1, E=0)
    {
        std::vector<std::vector<int>> adj(1);
        CSRGraph g(1, adj);
        BFSResult seq = sequential_bfs(g, 0);
        BFSResult par = parallel_bfs_omp(g, 0, 4);
        bool ok = verify_bfs_results(seq, par) && seq.distance[0] == 0;
        std::cout << " Test 1 [Single Vertex (V=1, E=0)]            : " << (ok ? "[PASSED]" : "[FAILED]") << "\n";
        all_passed = all_passed && ok;
    }

    // Test 2: Disconnected Graph (V=6, two disconnected triangles {0,1,2} and {3,4,5})
    {
        std::vector<std::vector<int>> adj(6);
        adj[0] = {1, 2}; adj[1] = {0, 2}; adj[2] = {0, 1};
        adj[3] = {4, 5}; adj[4] = {3, 5}; adj[5] = {3, 4};
        CSRGraph g(6, adj);
        BFSResult seq = sequential_bfs(g, 0);
        BFSResult par = parallel_bfs_omp(g, 0, 4);
        bool ok = verify_bfs_results(seq, par) && (seq.distance[3] == -1); // Component {3,4,5} unreachable
        std::cout << " Test 2 [Disconnected Graph (Unreachable Nodes)]: " << (ok ? "[PASSED]" : "[FAILED]") << "\n";
        all_passed = all_passed && ok;
    }

    // Test 3: Linear Chain Graph (V=100, path 0-1-2-...-99)
    {
        int V = 100;
        std::vector<std::vector<int>> adj(V);
        for (int i = 0; i < V - 1; ++i) {
            adj[i].push_back(i + 1);
            adj[i + 1].push_back(i);
        }
        CSRGraph g(V, adj);
        BFSResult seq = sequential_bfs(g, 0);
        BFSResult par = parallel_bfs_omp(g, 0, 4);
        bool ok = verify_bfs_results(seq, par) && (seq.distance[99] == 99);
        std::cout << " Test 3 [Linear Chain Graph (Max Diameter D=99)]: " << (ok ? "[PASSED]" : "[FAILED]") << "\n";
        all_passed = all_passed && ok;
    }

    // Test 4: Fully Connected Clique Graph (V=50)
    {
        int V = 50;
        std::vector<std::vector<int>> adj(V);
        for (int i = 0; i < V; ++i) {
            for (int j = 0; j < V; ++j) {
                if (i != j) adj[i].push_back(j);
            }
        }
        CSRGraph g(V, adj);
        BFSResult seq = sequential_bfs(g, 0);
        BFSResult par = parallel_bfs_omp(g, 0, 4);
        bool ok = verify_bfs_results(seq, par) && (seq.distance[49] == 1);
        std::cout << " Test 4 [Dense Clique Graph (Diameter D=1)]     : " << (ok ? "[PASSED]" : "[FAILED]") << "\n";
        all_passed = all_passed && ok;
    }

    // Test 5: Start Node with Zero Degree in mixed graph
    {
        int V = 5;
        std::vector<std::vector<int>> adj(V);
        adj[1] = {2}; adj[2] = {1}; // Node 0 has 0 degree
        CSRGraph g(V, adj);
        BFSResult seq = sequential_bfs(g, 0);
        BFSResult par = parallel_bfs_omp(g, 0, 4);
        bool ok = verify_bfs_results(seq, par) && (seq.distance[0] == 0 && seq.distance[1] == -1);
        std::cout << " Test 5 [Isolated Start Node (Degree 0)]       : " << (ok ? "[PASSED]" : "[FAILED]") << "\n";
        all_passed = all_passed && ok;
    }

    std::cout << "========================================================\n\n";
    return all_passed;
}
