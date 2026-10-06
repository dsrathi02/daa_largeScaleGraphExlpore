#include "graph.hpp"
#include "bfs.hpp"
#include <iostream>
#include <string>
#include <iomanip>
#include <cstdlib>
#include <windows.h>

void print_usage(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " [options]\n"
              << "Options:\n"
              << "  -type <erdos|scale_free>  Graph type (default: scale_free)\n"
              << "  -v <num_vertices>         Number of vertices (default: 100000)\n"
              << "  -d <avg_degree>           Average degree (default: 16)\n"
              << "  -threads <num_threads>    Number of parallel threads (default: max cores)\n"
              << "  -seed <seed_val>          Random seed (default: 42)\n"
              << "  -csv                      Output single CSV line format for benchmarking\n";
}

int main(int argc, char* argv[]) {
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    int max_hardware_threads = sysinfo.dwNumberOfProcessors;

    std::string graph_type = "scale_free";
    int V = 100000;
    int avg_degree = 16;
    int num_threads = max_hardware_threads;
    unsigned int seed = 42;
    bool csv_mode = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-type" && i + 1 < argc) {
            graph_type = argv[++i];
        } else if (arg == "-v" && i + 1 < argc) {
            V = std::atoi(argv[++i]);
        } else if (arg == "-d" && i + 1 < argc) {
            avg_degree = std::atoi(argv[++i]);
        } else if (arg == "-threads" && i + 1 < argc) {
            num_threads = std::atoi(argv[++i]);
        } else if (arg == "-seed" && i + 1 < argc) {
            seed = std::atoi(argv[++i]);
        } else if (arg == "-csv") {
            csv_mode = true;
        } else if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        }
    }

    // 1. Generate Graph
    CSRGraph graph;
    if (graph_type == "erdos") {
        graph = CSRGraph::generate_erdos_renyi(V, avg_degree, seed);
    } else {
        graph = CSRGraph::generate_scale_free(V, avg_degree / 2, seed);
    }

    double csr_mem_mb = (graph.offsets.size() * sizeof(long long) + graph.edges.size() * sizeof(int)) / (1024.0 * 1024.0);

    // Choose start vertex with degree > 0
    int start_node = 0;
    for (int i = 0; i < V; ++i) {
        if (graph.degree(i) > 0) {
            start_node = i;
            break;
        }
    }

    // 2. Run Sequential BFS
    BFSResult seq_res = sequential_bfs(graph, start_node);

    // 3. Run Parallel BFS
    BFSResult par_res = parallel_bfs(graph, start_node, num_threads);

    // 4. Verify Correctness
    bool is_correct = verify_bfs_results(seq_res, par_res);

    // Calculate metrics
    double speedup = (par_res.execution_time_ms > 0) ? (seq_res.execution_time_ms / par_res.execution_time_ms) : 1.0;
    double efficiency = speedup / num_threads;

    if (csv_mode) {
        // Output CSV format:
        // GraphType,Vertices,Edges,CSRMemMB,Threads,SeqTimeMS,ParTimeMS,Speedup,Efficiency,SeqMTEPS,ParMTEPS,Verified
        std::cout << graph_type << ","
                  << V << ","
                  << graph.num_edges << ","
                  << std::fixed << std::setprecision(2) << csr_mem_mb << ","
                  << num_threads << ","
                  << std::setprecision(4) << seq_res.execution_time_ms << ","
                  << par_res.execution_time_ms << ","
                  << speedup << ","
                  << efficiency << ","
                  << seq_res.mteps << ","
                  << par_res.mteps << ","
                  << (is_correct ? "PASSED" : "FAILED") << "\n";
    } else {
        std::cout << "========================================================\n";
        std::cout << "           LARGE-SCALE GRAPH EXPLORATION (BFS)          \n";
        std::cout << "========================================================\n";
        std::cout << " Graph Type         : " << graph_type << "\n";
        std::cout << " Vertices (V)       : " << V << "\n";
        std::cout << " Edges (E)          : " << graph.num_edges << "\n";
        std::cout << " CSR Memory Footprint: " << std::fixed << std::setprecision(2) << csr_mem_mb << " MB\n";
        std::cout << " Start Vertex       : " << start_node << "\n";
        std::cout << " Parallel Threads   : " << num_threads << "\n";
        std::cout << "--------------------------------------------------------\n";
        std::cout << " Sequential Time    : " << std::setprecision(3) << seq_res.execution_time_ms << " ms (" << seq_res.mteps << " MTEPS)\n";
        std::cout << " Parallel Time      : " << par_res.execution_time_ms << " ms (" << par_res.mteps << " MTEPS)\n";
        std::cout << " Speedup            : " << std::setprecision(2) << speedup << "x\n";
        std::cout << " Parallel Efficiency: " << std::setprecision(2) << (efficiency * 100.0) << "%\n";
        std::cout << " Correctness Check  : " << (is_correct ? "[PASSED] (Sequential & Parallel outputs match identically!)" : "[FAILED]") << "\n";
        std::cout << "========================================================\n";
    }

    return is_correct ? 0 : 1;
}
