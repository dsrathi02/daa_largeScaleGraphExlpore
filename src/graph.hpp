#ifndef GRAPH_HPP
#define GRAPH_HPP

#include <vector>
#include <iostream>
#include <random>
#include <algorithm>
#include <numeric>
#include <chrono>
#include <fstream>

// Compressed Sparse Row (CSR) Graph Representation
struct CSRGraph {
    int num_vertices;
    long long num_edges; // Total directed edges stored (undirected edges count twice)
    std::vector<long long> offsets; // Size: num_vertices + 1
    std::vector<int> edges;         // Size: num_edges

    CSRGraph() : num_vertices(0), num_edges(0) {}

    CSRGraph(int V, const std::vector<std::vector<int>>& adj) {
        num_vertices = V;
        offsets.resize(V + 1, 0);
        long long total_edges = 0;
        for (int i = 0; i < V; ++i) {
            offsets[i] = total_edges;
            total_edges += adj[i].size();
        }
        offsets[V] = total_edges;
        num_edges = total_edges;

        edges.resize(num_edges);
        for (int i = 0; i < V; ++i) {
            long long start = offsets[i];
            for (size_t j = 0; j < adj[i].size(); ++j) {
                edges[start + j] = adj[i][j];
            }
        }
    }

    inline int degree(int u) const {
        return static_cast<int>(offsets[u + 1] - offsets[u]);
    }

    inline const int* neighbors(int u) const {
        return &edges[offsets[u]];
    }

    // Generator 1: Erdős-Rényi Random Graph (Uniform Degree Distribution)
    static CSRGraph generate_erdos_renyi(int V, int avg_degree, unsigned int seed = 42) {
        std::mt19937 gen(seed);
        std::vector<std::vector<int>> adj(V);
        long long target_edges = (static_cast<long long>(V) * avg_degree) / 2;

        std::uniform_int_distribution<int> dis(0, V - 1);
        long long added = 0;

        while (added < target_edges) {
            int u = dis(gen);
            int v = dis(gen);
            if (u != v) {
                adj[u].push_back(v);
                adj[v].push_back(u);
                added++;
            }
        }

        // Sort and remove duplicates for clean graph topology
        for (int i = 0; i < V; ++i) {
            std::sort(adj[i].begin(), adj[i].end());
            adj[i].erase(std::unique(adj[i].begin(), adj[i].end()), adj[i].end());
        }

        return CSRGraph(V, adj);
    }

    // Generator 2: Scale-Free / Power-Law Graph (Barabási-Albert Model)
    static CSRGraph generate_scale_free(int V, int m_attachments, unsigned int seed = 42) {
        std::mt19937 gen(seed);
        std::vector<std::vector<int>> adj(V);
        std::vector<int> repeated_nodes;

        int m0 = std::max(2, m_attachments);
        repeated_nodes.reserve(V * m0 * 2);

        // Initial clique of m0 nodes
        for (int i = 0; i < m0; ++i) {
            for (int j = i + 1; j < m0; ++j) {
                adj[i].push_back(j);
                adj[j].push_back(i);
                repeated_nodes.push_back(i);
                repeated_nodes.push_back(j);
            }
        }

        // Preferential attachment for remaining nodes
        for (int i = m0; i < V; ++i) {
            int targets_added = 0;
            std::vector<int> chosen;
            chosen.reserve(m0);

            while (targets_added < m0 && !repeated_nodes.empty()) {
                std::uniform_int_distribution<size_t> dis(0, repeated_nodes.size() - 1);
                int target = repeated_nodes[dis(gen)];

                if (target != i && std::find(chosen.begin(), chosen.end(), target) == chosen.end()) {
                    adj[i].push_back(target);
                    adj[target].push_back(i);
                    chosen.push_back(target);
                    targets_added++;
                }
            }

            for (int target : chosen) {
                repeated_nodes.push_back(i);
                repeated_nodes.push_back(target);
            }
        }

        // Sort and remove duplicates
        for (int i = 0; i < V; ++i) {
            std::sort(adj[i].begin(), adj[i].end());
            adj[i].erase(std::unique(adj[i].begin(), adj[i].end()), adj[i].end());
        }

        return CSRGraph(V, adj);
    }
};

#endif // GRAPH_HPP
