# Q16. Large-Scale Graph Exploration

> **Computational Framework for Exploring Large Graphs using OpenMP Level-Synchronous Parallel Breadth-First Search (BFS) in Compressed Sparse Row (CSR) Format.**

---

## 1. Problem Definition & Overview

Sequential processing of large-scale graphs (social networks, web graphs, biological networks) is fundamentally bottlenecked by **memory latency** and **irregular data access patterns**. As graph sizes exceed millions of vertices and tens of millions of edges, standard node dynamic structures (`std::vector<std::vector<int>>`) suffer from severe cache line invalidations, heap fragmentation, and queue contention.

This project delivers a high-performance C++ & Python computational framework:
1. Implements **Compressed Sparse Row (CSR)** graph storage to maximize spatial cache locality and eliminate pointer chasing overhead.
2. Features **Erdős-Rényi (Uniform)** and **Scale-Free / Power-Law (Barabási-Albert)** synthetic graph generators up to **1,000,000 vertices**.
3. Implements **Level-Synchronous Parallel BFS** using **OpenMP**, dynamic chunk load balancing (`#pragma omp for schedule(dynamic, 512)`), thread-local frontier buffering, and atomic Compare-And-Swap (CAS) state claims.
4. Uses a rigorous benchmark methodology (1 un-timed warm-up run + 5 repeated executions taking the **median** time) measuring Traversed Edges Per Second (**MTEPS**), Speedup ($S$), Parallel Efficiency ($E$), and scaling.
5. **Every benchmark run is checked element-wise against the sequential result**, supported by an edge-case and 2/4/8-thread race condition stress unit testing suite.

---

## 2. Model Justification: Why OpenMP (vs MPI & CUDA)?

- **Shared-Memory Architecture:** Graph exploration algorithms require concurrent access to global graph topology arrays (`offsets`, `edges`) and vertex state arrays (`distance`). OpenMP provides lightweight multi-threading on shared memory without IPC overhead.
- **Why Not MPI?** MPI is engineered for distributed-memory clusters and requires explicit graph partition management and high network latency per BFS level. On a single multi-core node, shared-memory OpenMP eliminates message passing overhead and partition load imbalance.
- **Why Not CUDA (GPU)?** CUDA excels at regular fine-grained dense arithmetic, but irregular graph traversals suffer from severe GPU warp thread divergence, uncoalesced global memory access, and host-to-device PCI-e transfer bottlenecks.
- **Dynamic Load Balancing (`schedule(dynamic, 512)`):** Real-world power-law graphs exhibit extreme degree skew (hub vertices have thousands of edges while minor vertices have 2 edges). OpenMP's dynamic loop scheduler distributes vertex blocks dynamically to idle threads, eliminating thread starvation.

---

## 3. Mathematical Proofs & Theoretical Foundations

### 3.1 Memory Efficiency of CSR vs Standard Adjacency List
Let $V$ be the number of vertices and $E$ be the number of directed edges. Binary megabytes ($\text{MiB} = 1024^2\text{ bytes}$) are used consistently across calculations and program logging.

- **CSR Memory Footprint ($M_{\text{CSR}}$):**
  $$M_{\text{CSR}} = \frac{(V + 1) \times 8\text{ bytes (offsets)} + E \times 4\text{ bytes (edges)}}{1024 \times 1024}\text{ MiB}$$

- **Standard Adjacency List Footprint ($M_{\text{AdjList}}$):**
  $$M_{\text{AdjList}} = \frac{V \times 24\text{ bytes (std::vector headers)} + E \times 4\text{ bytes (elements)}}{1024 \times 1024}\text{ MiB}$$

- **Exact Memory Savings Formula:**
  $$\text{Memory Reduction (\%)} = \left( 1 - \frac{M_{\text{CSR}}}{M_{\text{AdjList}}} \right) \times 100\%$$

  For $V = 100,000$ and $E = 1,599,928$, CSR uses **6.87 MiB** vs Adjacency List's **8.39 MiB**, yielding an **18.2% raw byte reduction** and **zero heap fragmentation**.

### 3.2 Single-Ownership Invariant (Race Condition Prevention)
When multiple worker threads attempt to visit unvisited neighbor vertex $v$ concurrently:
$$\text{CAS}(\&\text{distance}[v], -1, \text{level} + 1)$$
- `distance[v]` is initialized to `-1`.
- Atomic Compare-And-Swap succeeds for **exactly ONE thread**.
- That winning thread sets `distance[v] = level + 1` and enqueues $v$ into its thread-local buffer.
- All other losing threads observe `distance[v] != -1` and safely bypass $v$.
- **Proof:** This guarantees single-ownership invariant and prevents duplicate queue entries without lock contention.

---

## 4. Repository Structure

```
daa_prj/
├── src/
│   ├── graph.hpp               # CSR Data structure & Erdos-Renyi / Scale-Free graph generators
│   ├── bfs.hpp                 # BFS Result struct, sequential & OpenMP parallel BFS, stress tests
│   ├── bfs.cpp                 # OpenMP Level-Synchronous BFS, atomic CAS claim, median timing runner
│   └── main.cpp                # CLI entry point, edge-case test runner, and CSV output driver
├── scripts/
│   └── benchmark.py            # Rigorous 5-repeat median benchmark driver and matplotlib plot generator
├── results/
│   ├── benchmark_results.csv   # Measured empirical performance raw dataset
│   ├── chart_thread_scaling.png      # Median Speedup vs OpenMP Threads plot
│   ├── chart_graph_size_scaling.png  # Traversal Throughput (MTEPS) vs Graph Size plot
│   └── chart_execution_time.png      # Sequential vs Parallel median execution time bar chart
├── README.md                   # System documentation and viva guide
├── PROJECT_REPORT.md           # Formal academic project report
└── LLM_USAGE_LOG.md            # Mandatory LLM Prompt log with rubric table and case study
```

---

## 5. Build & Execution Instructions

### Hardware & Environment Specifications
- **CPU:** Multi-core x86_64 Processor (4 Logical Cores)
- **Compiler:** `g++ (MinGW.org GCC-6.3.0-1) 6.3.0` with `-O3 -fopenmp`
- **OS:** Windows 11 64-bit / Linux

### Windows Build Command (MinGW / MSVC)
```powershell
g++ -O3 -fopenmp -L. src/main.cpp src/bfs.cpp -o main.exe
```

### Linux Build Command (GCC)
```bash
g++ -O3 -fopenmp src/main.cpp src/bfs.cpp -o main
```

### Execution Commands
```powershell
# 1. Run Unit & 2/4/8-Thread Race Condition Stress Test Suite
.\main.exe -test

# 2. Run Single Execution CLI (100k Vertices, 4 Threads, Median of 5 Runs)
.\main.exe -type scale_free -v 100000 -d 16 -threads 4 -runs 5

# 3. Test Parameter Guard against invalid inputs (returns error and exits)
.\main.exe -threads 0

# 4. Run Automated Benchmark Suite
python scripts/benchmark.py
```

---

## 6. Viva Preparation Material

### Q1: What is the Graph500 benchmark and why is BFS used?
**Answer:** Graph500 is the international HPC benchmark for data-intensive supercomputing. BFS is used because it measures memory subsystem throughput (bandwidth, random access, latency) rather than floating-point performance. Traversals are measured in **MTEPS** (Million Traversed Edges Per Second).

### Q2: Why is CSR preferred over Adjacency Lists for large graphs?
**Answer:** Adjacency lists (`vector<vector<int>>`) store array pointers scattered across the heap, causing massive pointer-chasing and cache misses. CSR packs all edges into a single contiguous array (`edges`) and uses an `offsets` index array, maximizing L1/L2 cache prefetching and eliminating vector header memory overhead.

### Q3: How are race conditions prevented when multiple threads visit the same node?
**Answer:** We use an atomic Compare-And-Swap (CAS) operation (`__sync_bool_compare_and_swap`). When thread $A$ and thread $B$ inspect unvisited node $v$ (`distance[v] == -1`), only the single thread that successfully executes CAS from `-1` to `current_level + 1` receives ownership of $v$ and appends it to its thread-local buffer.

### Q4: Why don't threads push discovered nodes directly to a single shared queue?
**Answer:** Pushing directly to a single shared queue would require a mutex or atomic increment per edge, creating catastrophic lock contention. Instead, each thread writes to a **thread-private vector buffer** and merges its local buffer into the global frontier inside `#pragma omp critical` once per level.

### Q5: What is Amdahl's Law and why doesn't parallel BFS scale infinitely?
**Answer:** Amdahl's law states $S(P) = \frac{1}{(1-f) + \frac{f}{P}}$. Sequential components (level barriers, frontier allocation, memory bus bandwidth limits) constrain maximum speedup regardless of thread count.

---

## 7. Individual Contribution Statement

- **Dhanashree Rathi (Lead Architecture & CSR Implementation):** Designed Compressed Sparse Row (CSR) graph storage, mathematical memory proof, and synthetic graph generators (Erdős-Rényi and Barabási-Albert Scale-Free).
- **Parallel Optimization & Benchmarking Team:** Implemented OpenMP Level-Synchronous Parallel BFS, atomic CAS state claim, dynamic chunk scheduling, automated 5-repeat median benchmark suite, and Python performance plotting scripts.
- **Verification & Documentation Team:** Engineered edge-case and multi-threaded stress testing suite (`test_edge_cases`), authored Project Report, LLM Usage Log, and Viva Q&A guide.
