import os
import subprocess
import csv
import matplotlib.pyplot as plt

def run_benchmarks():
    os.makedirs("results", exist_ok=True)
    csv_file = os.path.join("results", "benchmark_results.csv")
    
    headers = [
        "GraphType", "Vertices", "Edges", "CSRMemMB", "Threads",
        "SeqTimeMS", "ParTimeMS", "Speedup", "Efficiency",
        "SeqMTEPS", "ParMTEPS", "Verified"
    ]
    
    records = []
    
    print("==================================================")
    print("      STARTING GRAPH EXPLORATION BENCHMARKS       ")
    print("==================================================")

    # Experiment 1: Thread Strong Scaling (V = 100,000, Threads = 1, 2, 4)
    print("\n--- Running Thread Scaling Experiment ---")
    for gtype in ["scale_free", "erdos"]:
        for threads in [1, 2, 3, 4]:
            cmd = [
                ".\\main.exe",
                "-type", gtype,
                "-v", "100000",
                "-d", "16",
                "-threads", str(threads),
                "-seed", "42",
                "-csv"
            ]
            res = subprocess.run(cmd, capture_output=True, text=True)
            if res.returncode == 0:
                line = res.stdout.strip()
                print(f"[{gtype.upper()}] Threads: {threads} | Output: {line}")
                parts = line.split(",")
                records.append(parts)
            else:
                print(f"Error running benchmark: {res.stderr}")

    # Experiment 2: Graph Size Weak/Strong Scaling (Threads = 4, V = 20k to 500k)
    print("\n--- Running Graph Size Scaling Experiment ---")
    sizes = [20000, 50000, 100000, 200000, 400000]
    for gtype in ["scale_free", "erdos"]:
        for v in sizes:
            cmd = [
                ".\\main.exe",
                "-type", gtype,
                "-v", str(v),
                "-d", "16",
                "-threads", "4",
                "-seed", "42",
                "-csv"
            ]
            res = subprocess.run(cmd, capture_output=True, text=True)
            if res.returncode == 0:
                line = res.stdout.strip()
                print(f"[{gtype.upper()}] V: {v} | Output: {line}")
                parts = line.split(",")
                records.append(parts)

    # Write CSV
    with open(csv_file, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(headers)
        writer.writerows(records)
    print(f"\nSaved benchmark results to {csv_file}")

    generate_plots(csv_file)

def generate_plots(csv_file):
    print("\n--- Generating Benchmark Performance Charts ---")
    
    rows = []
    with open(csv_file, "r") as f:
        reader = csv.DictReader(f)
        for r in reader:
            rows.append({
                "GraphType": r["GraphType"],
                "Vertices": int(r["Vertices"]),
                "Edges": int(r["Edges"]),
                "Threads": int(r["Threads"]),
                "SeqTimeMS": float(r["SeqTimeMS"]),
                "ParTimeMS": float(r["ParTimeMS"]),
                "Speedup": float(r["Speedup"]),
                "Efficiency": float(r["Efficiency"]),
                "SeqMTEPS": float(r["SeqMTEPS"]),
                "ParMTEPS": float(r["ParMTEPS"])
            })

    # Plot 1: Speedup vs Threads (V = 100,000)
    plt.figure(figsize=(8, 5))
    for gtype in ["scale_free", "erdos"]:
        filtered = [r for r in rows if r["GraphType"] == gtype and r["Vertices"] == 100000]
        # Sort by threads
        filtered.sort(key=lambda x: x["Threads"])
        threads = [r["Threads"] for r in filtered]
        speedups = [r["Speedup"] for r in filtered]
        
        label_name = "Scale-Free (Power-Law)" if gtype == "scale_free" else "Erdos-Renyi (Random)"
        plt.plot(threads, speedups, marker='o', linewidth=2, label=label_name)

    # Ideal linear speedup
    plt.plot([1, 4], [1, 4], linestyle='--', color='gray', label='Ideal Linear Speedup')
    plt.title("Thread Strong Scaling: Speedup vs Parallel Threads (V = 100,000)", fontsize=12, fontweight='bold')
    plt.xlabel("Number of Threads", fontsize=10)
    plt.ylabel("Speedup (x)", fontsize=10)
    plt.grid(True, linestyle=':', alpha=0.7)
    plt.legend()
    plt.tight_layout()
    chart1_path = os.path.join("results", "chart_thread_scaling.png")
    plt.savefig(chart1_path, dpi=300)
    plt.close()
    print(f"Saved plot: {chart1_path}")

    # Plot 2: Traversal Throughput (MTEPS) vs Graph Size (V) (Threads = 4)
    plt.figure(figsize=(8, 5))
    for gtype in ["scale_free", "erdos"]:
        filtered = [r for r in rows if r["GraphType"] == gtype and r["Threads"] == 4]
        filtered.sort(key=lambda x: x["Vertices"])
        vertices = [r["Vertices"] for r in filtered]
        par_mteps = [r["ParMTEPS"] for r in filtered]
        seq_mteps = [r["SeqMTEPS"] for r in filtered]

        label_name = "Scale-Free" if gtype == "scale_free" else "Erdos-Renyi"
        plt.plot(vertices, par_mteps, marker='s', linewidth=2, label=f"{label_name} (Parallel 4 Threads)")
        plt.plot(vertices, seq_mteps, marker='x', linestyle=':', label=f"{label_name} (Sequential)")

    plt.title("Traversal Throughput (MTEPS) vs Graph Size (Vertices)", fontsize=12, fontweight='bold')
    plt.xlabel("Graph Vertices (V)", fontsize=10)
    plt.ylabel("Traversed Throughput (MTEPS)", fontsize=10)
    plt.grid(True, linestyle=':', alpha=0.7)
    plt.legend()
    plt.tight_layout()
    chart2_path = os.path.join("results", "chart_graph_size_scaling.png")
    plt.savefig(chart2_path, dpi=300)
    plt.close()
    print(f"Saved plot: {chart2_path}")

    # Plot 3: Execution Time Comparison (Sequential vs Parallel 4 Threads)
    plt.figure(figsize=(8, 5))
    filtered_sf = [r for r in rows if r["GraphType"] == "scale_free" and r["Threads"] == 4]
    filtered_sf.sort(key=lambda x: x["Vertices"])
    vertices = [r["Vertices"] for r in filtered_sf]
    seq_times = [r["SeqTimeMS"] for r in filtered_sf]
    par_times = [r["ParTimeMS"] for r in filtered_sf]

    plt.bar([x - 5000 for x in vertices], seq_times, width=10000, label="Sequential Execution Time", color='#e74c3c', alpha=0.85)
    plt.bar([x + 5000 for x in vertices], par_times, width=10000, label="Parallel Execution Time (4 Threads)", color='#2ecc71', alpha=0.85)

    plt.title("Execution Time Comparison: Sequential vs Parallel BFS (Scale-Free)", fontsize=12, fontweight='bold')
    plt.xlabel("Graph Vertices (V)", fontsize=10)
    plt.ylabel("Execution Time (ms)", fontsize=10)
    plt.grid(True, linestyle=':', alpha=0.7)
    plt.legend()
    plt.tight_layout()
    chart3_path = os.path.join("results", "chart_execution_time.png")
    plt.savefig(chart3_path, dpi=300)
    plt.close()
    print(f"Saved plot: {chart3_path}")

if __name__ == "__main__":
    run_benchmarks()
