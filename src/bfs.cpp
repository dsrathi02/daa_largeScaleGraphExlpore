#include "bfs.hpp"
#include <queue>
#include <chrono>
#include <iostream>
#include <algorithm>
#include <vector>
#include <windows.h>

BFSResult sequential_bfs(const CSRGraph& graph, int start_node) {
    BFSResult result;
    int V = graph.num_vertices;
    result.distance.assign(V, -1);

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

// Worker context for level-synchronous parallel BFS
struct BFSWorkerContext {
    int thread_id;
    const CSRGraph* graph;
    const std::vector<int>* frontier;
    std::vector<int>* distance;
    int current_level;
    volatile LONG* work_counter;
    int chunk_size;

    std::vector<int> local_frontier;
    long long local_traversed_edges;
};

DWORD WINAPI BFSWorkerThread(LPVOID lpParam) {
    BFSWorkerContext* ctx = static_cast<BFSWorkerContext*>(lpParam);
    ctx->local_frontier.clear();
    ctx->local_traversed_edges = 0;

    size_t frontier_size = ctx->frontier->size();

    while (true) {
        // Atomic dynamic work assignment
        LONG start_idx = InterlockedExchangeAdd(ctx->work_counter, ctx->chunk_size);
        if (start_idx >= static_cast<LONG>(frontier_size)) {
            break; // No more work in current level
        }

        LONG end_idx = (std::min)(start_idx + ctx->chunk_size, static_cast<LONG>(frontier_size));

        for (LONG i = start_idx; i < end_idx; ++i) {
            int u = (*ctx->frontier)[i];
            long long graph_start = ctx->graph->offsets[u];
            long long graph_end = ctx->graph->offsets[u + 1];
            ctx->local_traversed_edges += (graph_end - graph_start);

            for (long long j = graph_start; j < graph_end; ++j) {
                int v = ctx->graph->edges[j];

                // Atomic CAS check to claim unvisited vertex v
                if ((*ctx->distance)[v] == -1) {
                    if (InterlockedCompareExchange((volatile LONG*)&((*ctx->distance)[v]), ctx->current_level + 1, -1) == -1) {
                        ctx->local_frontier.push_back(v);
                    }
                }
            }
        }
    }

    return 0;
}

BFSResult parallel_bfs(const CSRGraph& graph, int start_node, int num_threads) {
    if (num_threads <= 0) {
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        num_threads = sysinfo.dwNumberOfProcessors;
    }

    BFSResult result;
    int V = graph.num_vertices;
    result.distance.assign(V, -1);

    std::vector<int> frontier;
    std::vector<int> next_frontier;

    auto start_time = std::chrono::high_resolution_clock::now();

    result.distance[start_node] = 0;
    frontier.push_back(start_node);

    long long total_traversed_edges = 0;
    int current_level = 0;

    // Single-thread fast path optimization
    if (num_threads == 1) {
        return sequential_bfs(graph, start_node);
    }

    std::vector<HANDLE> handles(num_threads);
    std::vector<BFSWorkerContext> contexts(num_threads);

    while (!frontier.empty()) {
        next_frontier.clear();
        volatile LONG work_counter = 0;
        int chunk_size = 512; // Dynamic load balancing chunk size

        for (int t = 0; t < num_threads; ++t) {
            contexts[t].thread_id = t;
            contexts[t].graph = &graph;
            contexts[t].frontier = &frontier;
            contexts[t].distance = &result.distance;
            contexts[t].current_level = current_level;
            contexts[t].work_counter = &work_counter;
            contexts[t].chunk_size = chunk_size;

            handles[t] = CreateThread(NULL, 0, BFSWorkerThread, &contexts[t], 0, NULL);
        }

        // Wait for all worker threads to complete level traversal
        WaitForMultipleObjects(num_threads, handles.data(), TRUE, INFINITE);

        // Close thread handles & combine local frontiers
        for (int t = 0; t < num_threads; ++t) {
            CloseHandle(handles[t]);
            total_traversed_edges += contexts[t].local_traversed_edges;

            next_frontier.insert(next_frontier.end(), 
                                 contexts[t].local_frontier.begin(), 
                                 contexts[t].local_frontier.end());
        }

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
