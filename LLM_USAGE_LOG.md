# LLM Usage Log

## Project: Q16 - Large-Scale Graph Exploration
**Academic Discipline:** Design & Analysis of Algorithms / High-Performance Parallel Computing

---

## 1. Development Prompt Record

| Purpose | Actual Prompt | How Response Was Used | Modification / Verification |
| :--- | :--- | :--- | :--- |
| **Graph Representation** | "What is the optimal memory-efficient data structure for exploring massive graphs with millions of edges in C++?" | Guided decision to adopt Compressed Sparse Row (CSR) array layout over dynamic adjacency vectors. | Modified to include explicit memory size calculation (`csr_mem_mb`) for viva defense logging. Verified memory allocation using runtime stats. |
| **Parallel BFS Architecture** | "How to parallelize Breadth-First Search (BFS) for shared-memory multi-core architectures?" | Used to design Level-Synchronous frontier expansion algorithm with atomic status tracking. | Modified OpenMP pragmas to a custom Windows lock-free atomic thread pool (`InterlockedCompareExchange` & `InterlockedExchangeAdd`) to eliminate MinGW compiler `libpthread` dependency. |
| **Correctness Verification** | "How to verify that parallel graph exploration matches sequential output?" | Implemented element-wise distance vector comparison (`verify_bfs_results`). | Direct implementation. Verified across $100\%$ of benchmark test cases (`PASSED`). |
| **Benchmark Metric Formulation** | "What metrics are standard in Graph500 graph traversal benchmarks?" | Incorporated Traversed Edges Per Second (MTEPS), Speedup ($S = T_1 / T_p$), and Parallel Efficiency ($E = S / p$). | Implemented calculation in `main.cpp` and `benchmark.py`. Verified formulas against Graph500 specification. |

---

## 2. Evaluation of Modified / Rejected AI Suggestion

### Case Study: Rejection of Dynamic Vector Resizing inside OpenMP Atomic Block

* **Original AI Suggestion:**
  ```cpp
  #pragma omp atomic capture
  {
      start_pos = next_frontier.size();
      next_frontier.resize(next_frontier.size() + thread_local_frontier.size());
  }
  ```

* **Why It Was Unsuitable / Failed:**
  1. OpenMP `#pragma omp atomic capture` is restricted by the C++ OpenMP language specification to atomic scalar assignment operations (e.g., `v = x; x += expr;`).
  2. Calling member functions such as `std::vector::resize()` inside an OpenMP atomic block causes immediate compilation errors (`invalid operator for #pragma omp atomic`).
  3. Dynamic vector reallocation inside thread loops introduces severe lock contention and memory allocation overhead.

* **How It Was Resolved & Verified:**
  1. **First Iteration:** Replaced with thread-local vector buffering combined with `#pragma omp critical` for merging local frontiers once per level.
  2. **Final HPC Iteration:** Engineered a zero-dependency dynamic worker thread pool using lock-free Windows primitives (`InterlockedExchangeAdd` for dynamic chunk distribution, `InterlockedCompareExchange` for atomic vertex state locking).
  3. **Verification:** Verified by compiling cleanly with `g++ -O3` without external thread libraries, running $100\%$ of test cases with zero race conditions, and verifying identical vertex distance output between sequential and parallel traversals.

---

## 3. Team Contribution Summary

- **Architecture & CSR Data Structure:** Designed contiguous memory storage for vertices and edges, reducing cache misses.
- **Sequential & Parallel BFS Engines:** Implemented single-threaded queue BFS and multi-threaded Level-Synchronous BFS.
- **Benchmark Suite & Visualizations:** Automated strong/weak scaling benchmark pipeline and plotted high-resolution performance figures.
- **Documentation & Viva Prep:** Authored technical report, bottleneck analysis, and viva questions/answers.
