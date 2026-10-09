#include "graph.hpp"
#include "bfs.hpp"
#include <iostream>
#include <string>
#include <iomanip>
#include <cstdlib>
#include <omp.h>

void print_usage(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " [options]\n"
              << "Options:\n"
              << "  -type <erdos|scale_free>  Graph type (default: scale_free)\n"
              << "  -v <num_vertices>         Number of vertices (default: 100000, try 10 or 20 for small output)\n"
              << "  -d <avg_degree>           Average degree (default: 16)\n"
              << "  -threads <num_threads>    Number of OpenMP threads (> 0 required. 1 = Sequential only)\n"
              << "  -seed <seed_val>          Random seed (default: 42)\n"
              << "  -runs <num_repeats>       Number of repeated runs for median timing (default: 5)\n"
              << "  -test                     Run edge-case and multi-thread stress unit tests\n"
              << "  -csv                      Output single CSV line format for benchmarking\n";
}

int main(int argc, char* argv[]) {
    std::string graph_type = "scale_free";
    int V = 100000;
    int avg_degree = 16;
    int num_threads = omp_get_max_threads();
    unsigned int seed = 42;
    int num_runs = 5;
    bool csv_mode = false;
    bool run_tests = false;

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
        } else if (arg == "-runs" && i + 1 < argc) {
            num_runs = std::atoi(argv[++i]);
        } else if (arg == "-test") {
            run_tests = true;
        } else if (arg == "-csv") {
            csv_mode = true;
        } else if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        }
    }

    if (run_tests) {
        bool passed = run_edge_case_tests();
        return passed ? 0 : 1;
    }

    // STRICT CHECK 1: If number of threads is 0 or negative, code MUST NOT run.
    if (num_threads <= 0) {
        std::cerr << "Error: Invalid thread count (" << num_threads << "). Number of threads (-threads) must be a positive integer (> 0). Code execution aborted.\n";
        print_usage(argv[0]);
        return 1;
    }

    // Input validation guard against invalid parameters (-v, -d, -runs)
    if (V <= 0 || avg_degree <= 0 || num_runs <= 0) {
        std::cerr << "Error: Invalid parameter value. -v, -d, and -runs must be positive integers (> 0).\n";
        print_usage(argv[0]);
        return 1;
    }

    // 1. Generate Graph ONCE using fixed seed
    CSRGraph graph;
    if (graph_type == "erdos") {
        graph = CSRGraph::generate_erdos_renyi(V, avg_degree, seed);
    } else {
        graph = CSRGraph::generate_scale_free(V, avg_degree / 2, seed);
    }

    // Memory footprint calculations
    size_t csr_bytes = (graph.offsets.size() * sizeof(long long)) + (graph.edges.size() * sizeof(int));
    double csr_mem_mib = csr_bytes / (1024.0 * 1024.0);

    size_t adj_list_bytes = (graph.num_vertices * sizeof(std::vector<int>)) + (graph.num_edges * sizeof(int));
    double adj_list_mem_mib = adj_list_bytes / (1024.0 * 1024.0);
    double exact_mem_savings_pct = (1.0 - (static_cast<double>(csr_bytes) / static_cast<double>(adj_list_bytes))) * 100.0;

    // Choose start vertex with degree > 0
    int start_node = 0;
    for (int i = 0; i < V; ++i) {
        if (graph.degree(i) > 0) {
            start_node = i;
            break;
        }
    }

    // 2. Run Sequential BFS (1 Warm-up + N repeats -> Median Execution Time)
    BFSResult seq_res = run_bfs_with_median_timing(graph, start_node, false, 1, num_runs);

    // 3. STRICT CHECK 2: If num_threads == 1, ONLY Sequential BFS runs!
    BFSResult par_res;
    if (num_threads == 1) {
        par_res = seq_res; // Single thread: Parallel BFS is bypassed, only Sequential BFS runs
    } else {
        // Run OpenMP Parallel BFS across num_threads (> 1)
        par_res = run_bfs_with_median_timing(graph, start_node, true, num_threads, num_runs);
    }

    // 4. STRICT CORRECTNESS GATE: Check if sequential and parallel outputs match 100% identically
    bool is_correct = par_res.all_runs_verified && verify_bfs_results(seq_res, par_res);

    if (!csv_mode) {
        std::cout << "========================================================\n";
        std::cout << "        LARGE-SCALE GRAPH EXPLORATION (OpenMP BFS)      \n";
        std::cout << "========================================================\n";
        std::cout << " Graph Type         : " << graph_type << "\n";
        std::cout << " Vertices (V)       : " << V << "\n";
        std::cout << " Edges (E)          : " << graph.num_edges << "\n";
        std::cout << " CSR Memory         : " << std::fixed << std::setprecision(2) << csr_mem_mib << " MiB\n";
        std::cout << " AdjList Memory     : " << adj_list_mem_mib << " MiB\n";
        std::cout << " CSR Memory Savings : " << std::setprecision(1) << exact_mem_savings_pct << "% reduction\n";
        std::cout << " Start Vertex       : " << start_node << "\n";
        std::cout << " Requested Threads  : " << num_threads << (num_threads == 1 ? " (Sequential BFS Only)" : "") << "\n";
        std::cout << " Benchmark Runs     : 1 Warm-up + " << num_runs << " Repeats (Median Time)\n";
        std::cout << "--------------------------------------------------------\n";
        
        if (num_threads == 1) {
            std::cout << " [MODE]: -threads 1 specified -> Running Sequential BFS ONLY.\n";
            std::cout << "         Parallel BFS execution bypassed.\n";
        }

        // Show BFS distance outputs for ALL vertices
        int print_count = V;
        std::cout << " Sequential BFS Distances (all " << print_count << " vertices):\n  [";
        for (int i = 0; i < print_count; ++i) {
            std::cout << seq_res.distance[i] << (i + 1 < print_count ? ", " : "");
        }
        std::cout << "]\n";

        if (num_threads > 1) {
            std::cout << " Parallel BFS Distances   (all " << print_count << " vertices):\n  [";
            for (int i = 0; i < print_count; ++i) {
                std::cout << par_res.distance[i] << (i + 1 < print_count ? ", " : "");
            }
            std::cout << "]\n";
        }
        std::cout << "--------------------------------------------------------\n";

        // Correctness Gate Check
        if (!is_correct) {
            std::cerr << " [CORRECTNESS CHECK]: [FAILED] Mismatch detected between Sequential and Parallel BFS!\n";
            std::cerr << " Halting execution. Performance calculations aborted due to correctness failure.\n";
            std::cout << "========================================================\n";
            return 1;
        }

        std::cout << " [CORRECTNESS CHECK]: [PASSED] (Sequential & Parallel outputs match 100% identically!)\n";
        std::cout << "--------------------------------------------------------\n";
    }

    // If correctness check failed, do NOT proceed with speedup calculation
    if (!is_correct) {
        return 1;
    }

    // 5. Calculate performance metrics ONLY AFTER correctness is verified
    double speedup = (par_res.execution_time_ms > 0) ? (seq_res.execution_time_ms / par_res.execution_time_ms) : 1.0;
    double efficiency = speedup / num_threads;

    if (csv_mode) {
        // Output CSV format:
        // GraphType,Vertices,Edges,CSRMemMiB,AdjListMemMiB,MemSavingsPct,Threads,SeqTimeMS,ParTimeMS,Speedup,Efficiency,SeqMTEPS,ParMTEPS,Verified
        std::cout << graph_type << ","
                  << V << ","
                  << graph.num_edges << ","
                  << std::fixed << std::setprecision(2) << csr_mem_mib << ","
                  << adj_list_mem_mib << ","
                  << exact_mem_savings_pct << ","
                  << num_threads << ","
                  << std::setprecision(4) << seq_res.execution_time_ms << ","
                  << par_res.execution_time_ms << ","
                  << speedup << ","
                  << efficiency << ","
                  << seq_res.mteps << ","
                  << par_res.mteps << ","
                  << "PASSED\n";
    } else {
        std::cout << " Sequential Time    : " << std::setprecision(3) << seq_res.execution_time_ms << " ms (" << seq_res.mteps << " MTEPS)\n";
        if (num_threads > 1) {
            std::cout << " Parallel Time      : " << par_res.execution_time_ms << " ms (" << par_res.mteps << " MTEPS)\n";
            std::cout << " Speedup            : " << std::setprecision(2) << speedup << "x\n";
            std::cout << " Parallel Efficiency: " << std::setprecision(2) << (efficiency * 100.0) << "%\n";
        } else {
            std::cout << " Mode Note          : Sequential Execution Only (-threads 1)\n";
        }
        std::cout << "========================================================\n";
    }

    return 0;
}
