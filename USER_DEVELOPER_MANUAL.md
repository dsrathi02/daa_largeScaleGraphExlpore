# User Manual & Developer Manual

> **Q16. Large-Scale Graph Exploration Framework**  
> High-Performance Computational Framework for Exploring Large Graphs using OpenMP Level-Synchronous Parallel BFS in Compressed Sparse Row (CSR) Format.

---

# PART I: USER MANUAL

## 1. System Requirements & Prerequisites

### Minimum Hardware
- **Processor:** Multi-core x86_64 CPU (2 or more physical/logical cores).
- **RAM:** 4 GB minimum (8 GB+ recommended for graph sizes $V \ge 1,000,000$).
- **Storage:** 50 MB available disk space.

### Software Environment
- **Operating System:** Windows 10/11 64-bit, Linux (Ubuntu/Debian/Fedora/RHEL), or macOS.
- **C++ Compiler:** `g++` (GCC 6.0 or higher) with OpenMP support (`-fopenmp`).
- **Python (Optional for benchmarking/plotting):** Python 3.8+ with `matplotlib` installed (`python -m pip install matplotlib`).

---

## 2. Compilation & Building

### Building on Windows (MinGW / MSVC)
Open PowerShell or Command Prompt in the project root directory and run:
```powershell
g++ -O3 -fopenmp -L. src/main.cpp src/bfs.cpp -o main.exe
```

### Building on Linux / macOS (GCC)
Open bash or zsh in the project root directory and run:
```bash
g++ -O3 -fopenmp src/main.cpp src/bfs.cpp -o main
```

---

## 3. Command-Line Options (CLI Reference)

| Option | Argument Format | Default Value | Description |
| :--- | :--- | :--- | :--- |
| `-type` | `scale_free` \| `erdos` | `scale_free` | Graph topology model (Barabási-Albert Scale-Free or Erdős-Rényi Uniform). |
| `-v` | `<int > 0>` | `100000` | Number of vertices in the graph ($V$). Try `-v 10` or `-v 20` for small array visual inspection. |
| `-d` | `<int > 0>` | `16` | Average vertex degree ($d_{avg}$). |
| `-threads`| `<int > 0>` | Max CPU Cores | Number of OpenMP parallel threads. **Must be > 0**. (If `-threads 1`, runs Sequential BFS only). |
| `-seed` | `<int>` | `42` | Random number generator seed for 100% reproducible graph generation. |
| `-runs` | `<int > 0>` | `5` | Number of repeated runs to calculate **median execution time** (after 1 warm-up run). |
| `-test` | None | N/A | Executes the automated Unit & Multi-Threaded Stress Test Suite (Tests 1–6). |
| `-csv` | None | N/A | Formats output as a single comma-separated line for automated benchmarking scripts. |
| `-h` | None | N/A | Displays command-line help and usage instructions. |

---

## 4. Execution Examples

### Example 1: Run Small Graph Inspection ($V = 10$, 4 Threads)
Displays all vertex distance arrays side-by-side for visual inspection:
```powershell
.\main.exe -v 10 -threads 4
```

### Example 2: Run Standard Large Graph Execution ($V = 100,000$, 4 Threads, 5 Repeats Median)
```powershell
.\main.exe -type scale_free -v 100000 -d 16 -threads 4 -runs 5
```

### Example 3: Test Input Parameter Guard (Thread 0 or Negative Threads)
Aborts immediately with a descriptive error message:
```powershell
.\main.exe -threads 0
```

### Example 4: Run Sequential-Only Execution (`-threads 1`)
Bypasses Parallel BFS and runs Sequential BFS only:
```powershell
.\main.exe -v 100000 -threads 1
```

### Example 5: Run Automated Unit & Stress Test Suite
Tests 5 edge cases and a 15-run multi-threaded race condition stress test (2, 4, and 8 threads):
```powershell
.\main.exe -test
```

### Example 6: Run Automated Rigorous Benchmark Suite & Generate Charts
Sweeps graph sizes up to $V = 1,000,000$ and thread counts $1..4$, logging results to `results/benchmark_results.csv` and generating 3 PNG plots:
```powershell
python scripts/benchmark.py
```

---

## 5. Understanding the Console Output

```text
========================================================
        LARGE-SCALE GRAPH EXPLORATION (OpenMP BFS)      
========================================================
 Graph Type         : scale_free             # Graph generator model
 Vertices (V)       : 100000                 # Total graph vertices
 Edges (E)          : 1599928                # Total directed edges stored
 CSR Memory         : 6.87 MiB               # CSR storage memory footprint
 AdjList Memory     : 7.25 MiB               # Standard std::vector<vector<int>> footprint
 CSR Memory Savings : 5.3% reduction         # Memory savings percentage
 Start Vertex       : 0                      # Starting vertex ID for BFS traversal
 OpenMP Threads     : 4                      # Active OpenMP thread count
 Benchmark Runs     : 1 Warm-up + 5 Repeats  # Median calculation strategy
--------------------------------------------------------
 Sequential BFS Distances (all 100000 vertices):
  [0, 1, 1, 1, 1, 1, 1, 1, 1, 1, ...]        # Full distance array
 Parallel BFS Distances   (all 100000 vertices):
  [0, 1, 1, 1, 1, 1, 1, 1, 1, 1, ...]        # Parallel distance array
--------------------------------------------------------
 [CORRECTNESS CHECK]: [PASSED]               # 100% element-wise verification status
--------------------------------------------------------
 Sequential Time    : 20.485 ms (78.102 MTEPS)  # Sequential time & MTEPS throughput
 Parallel Time      : 11.208 ms (142.749 MTEPS) # Parallel time & MTEPS throughput
 Speedup            : 1.83x                  # Speedup factor (Seq Time / Par Time)
 Parallel Efficiency: 45.69%                 # Efficiency (Speedup / Threads)
========================================================
```

---
---

# PART II: DEVELOPER MANUAL

## 1. Repository Architecture & Directory Structure

```
daa_prj/
├── src/
│   ├── graph.hpp               # CSR Data structure & synthetic graph generators
│   ├── bfs.hpp                 # Struct definitions, BFS function prototypes, test declarations
│   ├── bfs.cpp                 # Sequential BFS, OpenMP Parallel BFS, median timing runner, stress test
│   └── main.cpp                # CLI parser, parameter guards, correctness gate, output formatter
├── scripts/
│   └── benchmark.py            # Python automated benchmark suite and matplotlib plotter
├── results/
│   ├── benchmark_results.csv   # Raw measured performance dataset
│   ├── chart_thread_scaling.png      # Plot: Median Speedup vs OpenMP Threads
│   ├── chart_graph_size_scaling.png  # Plot: Traversal Throughput (MTEPS) vs Graph Size
│   └── chart_execution_time.png      # Plot: Sequential vs Parallel execution time comparison
├── README.md                   # System documentation & viva guide
├── PROJECT_REPORT.md           # Academic project report
├── LLM_USAGE_LOG.md            # LLM Prompt Log with rubric table & case study
└── USER_DEVELOPER_MANUAL.md    # Comprehensive User & Developer Manual
```

---

## 2. Core Data Structures & Formulations

### 2.1 Compressed Sparse Row (`CSRGraph` in `src/graph.hpp`)
Graph topology is stored in two 1D physical vector arrays:
```cpp
struct CSRGraph {
    int num_vertices;               // Total vertex count V
    long long num_edges;            // Total directed edge count E
    std::vector<long long> offsets; // Size: V + 1 (Start index for each vertex)
    std::vector<int> edges;         // Size: E (Destination vertex IDs)
};
```
- **Neighbor Slice Lookup:** Neighbors of vertex $u$ span `edges[offsets[u] ... offsets[u+1]-1]`.
- **Degree Computation:** `degree(u) = offsets[u+1] - offsets[u]`.

### 2.2 Memory Footprint Calculations (`src/main.cpp`)
Binary megabyte units ($\text{MiB} = 1,048,576\text{ bytes}$) are calculated as:
$$\text{CSR Bytes} = (V + 1) \times 8 + E \times 4$$
$$\text{AdjList Bytes} = V \times 24 + E \times 4$$
$$\text{Memory Reduction (\%)} = \left( 1 - \frac{\text{CSR Bytes}}{\text{AdjList Bytes}} \right) \times 100\%$$

---

## 3. Algorithm Implementation & Concurrency Model

### 3.1 Sequential BFS (`sequential_bfs` in `src/bfs.cpp`)
Standard queue-based Single-Threaded BFS:
1. `std::queue<int> q;` initialized with `start_node`. `distance[start_node] = 0`.
2. Pops vertex $u$, iterates through contiguous slice `edges[offsets[u] ... offsets[u+1]-1]`.
3. If `distance[v] == -1`, sets `distance[v] = distance[u] + 1` and pushes $v$ to queue.
4. Returns `BFSResult` containing full `distance` array, `execution_time_ms`, and `mteps`.

### 3.2 OpenMP Parallel BFS (`parallel_bfs_omp` in `src/bfs.cpp`)
Level-Synchronous Parallel BFS using shared-memory OpenMP multi-threading:

```cpp
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
                    // Atomic CAS Claim: Prevents race conditions without mutexes
                    if (__sync_bool_compare_and_swap(&result.distance[v], -1, current_level + 1)) {
                        thread_local_frontier.push_back(v);
                    }
                }
            }
        }

        // Merge thread-local frontier into global next_frontier once per level
        if (!thread_local_frontier.empty()) {
            #pragma omp critical
            {
                next_frontier.insert(next_frontier.end(), 
                                     thread_local_frontier.begin(), 
                                     thread_local_frontier.end());
            }
        }
    }

    total_traversed_edges += level_edges;
    frontier.swap(next_frontier);
    current_level++;
}
```

### 3.3 Atomic Single-Ownership Invariant (Race Condition Prevention)
When multiple threads concurrently discover unvisited vertex $v$:
- `distance[v]` is initialized to `-1`.
- `__sync_bool_compare_and_swap(&distance[v], -1, current_level + 1)` executes atomically at the hardware CPU instruction level.
- **Mathematical Invariant:** Exactly **ONE** thread successfully transitions `distance[v]` from `-1` to `current_level + 1`. That winning thread enqueues $v$ into its `thread_local_frontier`. All losing threads observe `distance[v] != -1` and safely bypass $v$.

---

## 4. Benchmark Automation & Plotting Pipeline (`scripts/benchmark.py`)

- **Experiment 1 (Strong Thread Scaling):** Sweeps threads $1, 2, 3, 4$ on fixed graph ($V = 100,000$, fixed seed `42`).
- **Experiment 2 (Weak/Graph Size Scaling):** Sweeps $V \in \{50k, 100k, 200k, 500k, 1M\}$ on 4 threads.
- **Median Timing Driver:** Executes 1 un-timed warm-up run + 5 repeated executions, logging median timing to `results/benchmark_results.csv`.
- **Visualization:** Uses `matplotlib` to render `chart_thread_scaling.png`, `chart_graph_size_scaling.png`, and `chart_execution_time.png`.

---

## 5. Developer Guide: How to Extend the Framework

### Adding a New Graph Data File Parser (e.g. SNAP / Matrix Market format)
1. In `src/graph.hpp`, add a static loader method:
   ```cpp
   static CSRGraph load_from_edge_list(const std::string& filename) {
       // Read (u, v) edge pairs from text file, build adjacency list, return CSRGraph(V, adj)
   }
   ```
2. In `src/main.cpp`, add a `-file <path>` CLI option to invoke `CSRGraph::load_from_edge_list(...)`.

### Adding a New Graph Algorithm (e.g. Parallel Connected Components / Dijkstra)
1. Declare function prototype in `src/bfs.hpp`.
2. Implement using CSR topology in `src/bfs.cpp`. CSR contiguous slices enable high-performance parallel array traversals across all graph algorithms.

---

## 6. Individual Contribution Statement

1. **Dhanashree Rathi — Architecture & Graph Representation:** Designed the overall architecture, implemented CSR graph storage, developed memory-efficiency calculations, and created synthetic graph generators (Erdős–Rényi and Barabási–Albert Scale-Free).
2. **Siya Daga — Parallel BFS Implementation:** Implemented level-synchronous parallel BFS using OpenMP, atomic Compare-And-Swap (CAS), dynamic scheduling, and thread-local frontier buffers.
3. **Vaidehi Sonawane — Benchmarking & Performance Analysis:** Developed the automated benchmarking workflow, implemented repeated-run median timing, collected performance metrics, and generated Python-based performance charts.
4. **Surabhi Singh — Testing & Documentation:** Developed edge-case validation, verified sequential and parallel BFS results, and prepared the project report, LLM Usage Log, and viva Q&A guide.
