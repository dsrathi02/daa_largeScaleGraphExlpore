# Project Report: Q16. Large-Scale Graph Exploration

**Course:** Design and Analysis of Algorithms / High-Performance Computing  
**Topic:** Computational Framework for Exploring and Analyzing Large-Scale Graphs using Level-Synchronous Parallel BFS  
**Authors:** Dhanashree Rathi & Team  

---

## 1. Abstract & Problem Definition

Sequential Breadth-First Search (BFS) on massive graphs with millions of vertices and edges becomes severely bottlenecked by **memory latency**, **pointer chasing**, and **cache line invalidation**. As graph sizes grow beyond memory cache limits, standard node representations (such as `std::vector<std::vector<int>>`) suffer from dynamic allocation overhead and non-contiguous memory accesses.

This project delivers an HPC computational framework for exploring large-scale graphs:
1. Employs **Compressed Sparse Row (CSR)** memory storage to eliminate pointer overhead and maximize spatial L1/L2 cache locality.
2. Implements **Level-Synchronous Parallel BFS** utilizing **OpenMP** shared-memory multi-threading, dynamic chunk load balancing (`#pragma omp for schedule(dynamic, 512)`), thread-local frontier buffering, and atomic Compare-And-Swap (CAS) state claims.
3. Incorporates a rigorous benchmark methodology (1 un-timed warm-up run + 5 repeated executions taking the **median** time) across Erdős-Rényi (Uniform) and Barabási-Albert (Scale-Free) graphs up to **1,000,000 vertices**.
4. Achieves **100% verified correctness** across single-threaded and multi-threaded runs, supported by an edge-case unit testing suite.

---

## 2. Graph Representation: Compressed Sparse Row (CSR) & Memory Analysis

### 2.1 CSR Storage Architecture
CSR packs the graph into two contiguous physical 1D arrays:
- `offsets` (Size $|V| + 1$): Stores the starting index in `edges` for vertex $u$. Neighbors of $u$ reside in `edges[offsets[u] ... offsets[u+1]-1]`.
- `edges` (Size $|E|$): Stores destination vertex IDs contiguously in memory.

### 2.2 Mathematical Proof of Memory Efficiency
Let $V$ be the number of vertices and $E$ be the number of directed edges.

* **CSR Memory Footprint ($M_{\text{CSR}}$):**
  $$M_{\text{CSR}} = (V + 1) \times 8\text{ bytes (long long offsets)} + E \times 4\text{ bytes (int edges)}$$

* **Standard Adjacency List Footprint ($M_{\text{AdjList}}$):**
  A standard `std::vector<std::vector<int>>` allocates a vector header per vertex (24 bytes on 64-bit platforms / 12 bytes on 32-bit platforms) plus element storage:
  $$M_{\text{AdjList}} = V \times 24\text{ bytes (vector header)} + E \times 4\text{ bytes (int elements)}$$

* **Exact Memory Reduction:**
  $$\text{Memory Savings (\%)} = \left( 1 - \frac{M_{\text{CSR}}}{M_{\text{AdjList}}} \right) \times 100\%$$

  For a graph with $V = 100,000$ and $E = 1,600,000$:
  - $M_{\text{AdjList}} = 100,000 \times 24 + 1,600,000 \times 4 = 2,400,000 + 6,400,000 = 8.80\text{ MB}$
  - $M_{\text{CSR}} = 100,001 \times 8 + 1,600,000 \times 4 = 800,008 + 6,400,000 = 7.20\text{ MB}$
  - **Exact Memory Reduction:** $\approx 18.2\%$ reduction in raw bytes, with **zero heap fragmentation**.

---

## 3. Parallel Algorithm Design & "Why OpenMP"

### 3.1 Why OpenMP?
OpenMP was selected as the parallel multi-threading model for the following engineering reasons:
1. **Direct Shared-Memory Access:** Graph exploration requires concurrent access to global graph structure arrays (`offsets`, `edges`) and the global level array (`distance`). OpenMP enables zero-copy shared-memory access across all CPU threads.
2. **Compiler-Level Pragmas:** Eliminates manually managing OS thread lifecycles, mutex locks, or low-level pthreads boilerplate.
3. **Dynamic Load Balancing (`schedule(dynamic, 512)`):** Real-world scale-free graphs contain hub vertices with high degrees alongside thousands of low-degree vertices. Static scheduling causes severe thread starvation. OpenMP's dynamic loop chunk scheduling distributes frontier vertex blocks dynamically to idle threads.

### 3.2 Level-Synchronous BFS Architecture
1. **Frontier Loop:** At level $k$, active vertices in `frontier` are expanded concurrently across worker threads:
   ```cpp
   #pragma omp parallel
   {
       std::vector<int> thread_local_frontier;
       #pragma omp for schedule(dynamic, 512) reduction(+:level_edges)
       for (size_t i = 0; i < frontier.size(); ++i) {
           int u = frontier[i];
           // Traverse neighbors of u
       }
   }
   ```
2. **Atomic Single-Ownership Invariant (Preventing Race Conditions):**
   When worker threads concurrently discover neighbor vertex $v$, they attempt an atomic Compare-And-Swap (CAS):
   $$\text{CAS}(\&\text{distance}[v], -1, \text{level} + 1)$$
   - **Mathematical Invariant:** `distance[v]` is initialized to `-1`. The atomic CAS succeeds for **exactly ONE thread**. That winning thread sets `distance[v] = level + 1` and appends $v$ to its `thread_local_frontier`. All losing threads observe `distance[v] != -1` and safely bypass $v$. This guarantees single-ownership and prevents duplicate queue entries without lock contention.
3. **Thread-Local Frontier Buffering:** Threads collect newly claimed nodes in private buffers (`thread_local_frontier`) and merge into global `next_frontier` inside `#pragma omp critical` once per level, reducing lock overhead from $\mathcal{O}(|E|)$ to $\mathcal{O}(P \times D)$.

---

## 4. Experimental Hardware & Compiler Environment

- **CPU:** Multi-core x86_64 Processor (4 Logical Processors)
- **RAM:** System DDR4 Memory
- **Operating System:** Windows 11 64-bit / Linux
- **Compiler:** `g++ (MinGW.org GCC-6.3.0-1) 6.3.0`
- **Compiler Flags:** `-O3 -fopenmp`

---

## 5. Build and Execution Instructions

### Windows Build Command (MinGW / GCC)
```powershell
g++ -O3 -fopenmp -L. src/main.cpp src/bfs.cpp -o main.exe
```

### Linux Build Command (GCC)
```bash
g++ -O3 -fopenmp src/main.cpp src/bfs.cpp -o main
```

### Execution Commands
```powershell
# Run Edge-Case Unit Test Suite
.\main.exe -test

# Single Execution CLI (100k Vertices, 4 OpenMP Threads, Median of 5 Runs)
.\main.exe -type scale_free -v 100000 -d 16 -threads 4 -runs 5

# Automated Rigorous Benchmark Suite
python scripts/benchmark.py
```

---

## 6. Edge-Case Unit Testing Suite

The framework includes an edge-case testing suite (`.\main.exe -test`):

1. **Test 1 [Single Vertex Graph ($V=1, E=0$)]**: Verified distance `dist[0] = 0`. `[PASSED]`
2. **Test 2 [Disconnected Graph]**: Verified unreachable components remain `dist[u] = -1`. `[PASSED]`
3. **Test 3 [Linear Chain Graph ($V=100$, Max Diameter $D=99$)]**: Verified max level `dist[99] = 99`. `[PASSED]`
4. **Test 4 [Dense Clique Graph ($V=50$, $E=2450$)]**: Verified all neighbors reachable at level 1. `[PASSED]`
5. **Test 5 [Isolated Start Node (Degree 0)]**: Verified traversal handles zero outgoing edges cleanly. `[PASSED]`

---

## 7. Individual Contribution Statement

- **Dhanashree Rathi (Lead Architecture & CSR Implementation):** Designed Compressed Sparse Row (CSR) graph storage, mathematical memory proof, and synthetic graph generators (Erdős-Rényi and Barabási-Albert Scale-Free).
- **Parallel Optimization & Benchmarking Team:** Implemented OpenMP Level-Synchronous Parallel BFS, atomic CAS state claim, dynamic chunk scheduling, automated 5-repeat median benchmark suite, and Python performance plotting scripts.
- **Verification & Documentation Team:** Engineered edge-case unit testing suite (`test_edge_cases`), authored Project Report, LLM Usage Log, and Viva Q&A guide.
