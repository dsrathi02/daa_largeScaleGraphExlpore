# LLM Usage Log

## Project: Q16 - Large-Scale Graph Exploration
**Academic Discipline:** Design & Analysis of Algorithms / High-Performance Parallel Computing

---

## 1. Development Prompt Record Table

| Sr. No. | Purpose | Actual Prompt | How Response Was Used | Modification / Verification |
| :---: | :--- | :--- | :--- | :--- |
| **1** | **Graph Representation** | "What is the optimal memory-efficient data structure for exploring massive graphs with millions of edges in C++?" | Adopted Compressed Sparse Row (CSR) array layout (`offsets` and `edges`) instead of `std::vector<std::vector<int>>`. | Added exact theoretical memory footprint formula (`csr_bytes` vs `adj_list_bytes`). Verified memory calculations at runtime. |
| **2** | **OpenMP Parallel BFS** | "How to parallelize Breadth-First Search (BFS) for shared-memory multi-core architectures using OpenMP?" | Designed Level-Synchronous frontier expansion algorithm using `#pragma omp parallel for schedule(dynamic, 512)` and atomic CAS claims. | Modified thread-local frontier merging logic using `#pragma omp critical` to prevent memory allocation race conditions. |
| **3** | **MinGW Build Debugging** | "Fix g++ compilation error cannot find -lpthread when linking OpenMP on Windows MinGW" | Identified missing pthreads spec link requirement on MinGW GCC 6.3.0. | Created empty stub library (`libpthread.a`) to allow `g++ -O3 -fopenmp -L.` to compile and link OpenMP binaries natively on Windows. Verified zero link errors. |
| **4** | **Timer Resolution Optimization** | "Sub-millisecond execution times rounding to 0.0000 ms in graph benchmark logging" | Switched execution timing from `std::chrono::milliseconds` to microsecond precision (`std::chrono::microseconds`). | Divided microsecond count by `1000.0` for sub-ms floating point resolution. Verified non-zero timing across small $V=20,000$ graphs. |
| **5** | **Edge-Case Validation** | "How to construct comprehensive unit tests for graph exploration edge cases in C++?" | Built automated unit test suite covering disconnected graphs, linear chains, cliques, and isolated nodes (`run_edge_case_tests()`). | Verified outputs element-wise (`verify_bfs_results`). All 5 edge-case tests returned `[PASSED]`. |
| **6** | **Benchmark Methodology** | "How to eliminate OS scheduling noise in parallel graph exploration benchmarks?" | Implemented 1 un-timed warm-up run followed by 5 repeated executions, taking the median time (`run_bfs_with_median_timing`). | Automated via `scripts/benchmark.py`. Verified stable, reproducible execution times across all thread configurations. |

---

## 2. Evaluation of Modified / Rejected AI Suggestion

### Case Study: Rejection of Dynamic Vector Resizing inside OpenMP Atomic Block

* **Why Considered:**
  During initial implementation of the Level-Synchronous Parallel BFS frontier expansion, an AI model suggested using `#pragma omp atomic capture` to atomically resize the global `next_frontier` array so that each thread could directly write its newly discovered vertices into assigned slice positions.

* **Original AI Suggestion:**
  ```cpp
  #pragma omp atomic capture
  {
      start_pos = next_frontier.size();
      next_frontier.resize(next_frontier.size() + thread_local_frontier.size());
  }
  ```

* **Why It Was Unsuitable / Failed:**
  1. OpenMP `#pragma omp atomic capture` is restricted by the C++ OpenMP language specification strictly to scalar assignment statements (e.g., `v = x; x += expr;`).
  2. Calling member functions such as `std::vector::resize()` inside an OpenMP atomic block causes immediate compilation errors (`invalid operator for #pragma omp atomic`).
  3. Dynamic vector reallocation inside thread loops introduces severe lock contention and memory allocation overhead.

* **Modification & Resolution:**
  1. Each thread collects newly discovered vertices into a thread-private vector buffer (`thread_local_frontier`).
  2. At the end of the level traversal loop, each thread appends its local buffer to `next_frontier` inside `#pragma omp critical`.
  3. Since this critical block executes at most ONCE per thread per level ($O(\text{num\_threads})$ operations per level), lock contention is completely eliminated during the main graph search loop.

* **How Verified:**
  Compiled cleanly with `g++ -O3 -fopenmp` without errors. Verified across all benchmark runs and edge-case unit tests (`PASSED`).

---

## 3. Individual Contribution Statement

1. **Dhanashree Rathi — Architecture & Graph Representation:** Designed the overall architecture, implemented CSR graph storage, developed memory-efficiency calculations, and created synthetic graph generators (Erdős–Rényi and Barabási–Albert Scale-Free).
2. **Siya Daga — Parallel BFS Implementation:** Implemented level-synchronous parallel BFS using OpenMP, atomic Compare-And-Swap (CAS), dynamic scheduling, and thread-local frontier buffers.
3. **Vaidehi Sonawane — Benchmarking & Performance Analysis:** Developed the automated benchmarking workflow, implemented repeated-run median timing, collected performance metrics, and generated Python-based performance charts.
4. **Surabhi Singh — Testing & Documentation:** Developed edge-case validation, verified sequential and parallel BFS results, and prepared the project report, LLM Usage Log, and viva Q&A guide.
