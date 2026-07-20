import os
import csv
import matplotlib.pyplot as plt

CSV_FILE = "D:/MFE Engine/reports/benchmarks_report.csv"
OUTPUT_DIR = "D:/MFE Engine/reports"

def generate_performance_plots():
    if not os.path.exists(CSV_FILE):
        print(f"Error: {CSV_FILE} does not exist. Run benchmark_harness.py first.")
        return

    benchmarks = []
    runtimes = []
    hpwls = []
    utilizations = []
    drcs = []
    memory = []

    with open(CSV_FILE, mode="r") as f:
        reader = csv.DictReader(f)
        for row in reader:
            if row["Success"] == "YES":
                benchmarks.append(row["Benchmark"])
                runtimes.append(float(row["Runtime (s)"]))
                hpwls.append(float(row["HPWL (um)"]))
                utilizations.append(float(row["Utilization (%)"]))
                drcs.append(int(row["DRC Violations"]))
                memory.append(float(row["Memory Usage (MB)"]))

    if not benchmarks:
        print("No successful benchmark data to plot.")
        return

    plt.style.use("ggplot")

    # Plot 1: Runtime Comparison
    plt.figure(figsize=(8, 5))
    plt.bar(benchmarks, runtimes, color="royalblue")
    plt.ylabel("Execution Runtime (seconds)")
    plt.title("MFE Benchmark Execution Speed (Lower is Better)")
    plt.tight_layout()
    plt.savefig(os.path.join(OUTPUT_DIR, "mfe_runtime_comparison.png"))
    plt.close()

    # Plot 2: HPWL (Wirelength) vs. Utilization Pareto Trade-off
    plt.figure(figsize=(8, 5))
    plt.scatter(hpwls, utilizations, color="crimson", s=150, zorder=5)
    for i, txt in enumerate(benchmarks):
        plt.annotate(txt, (hpwls[i], utilizations[i]), xytext=(5, 5), textcoords="offset points")
    plt.xlabel("Total Wirelength (HPWL in microns)")
    plt.ylabel("Utilization (%)")
    plt.title("MFE Pareto Trade-off: Wirelength vs. Utilization")
    plt.grid(True)
    plt.tight_layout()
    plt.savefig(os.path.join(OUTPUT_DIR, "mfe_wirelength_congestion_tradeoff.png"))
    plt.close()

    # Plot 3: Memory & DRC Metrics Subplots
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 5))
    
    ax1.bar(benchmarks, memory, color="seagreen")
    ax1.set_ylabel("Peak Memory Usage (MB)")
    ax1.set_title("RAM Footprint")

    ax2.bar(benchmarks, drcs, color="darkorange")
    ax2.set_ylabel("Total DRC Violations")
    ax2.set_title("Design Rule Violations (Ideally 0)")

    plt.suptitle("MFE Resource and Quality Metrics Across Benchmarks")
    plt.tight_layout()
    plt.savefig(os.path.join(OUTPUT_DIR, "mfe_resource_quality_metrics.png"))
    plt.close()

    print(f"[+] Performance plots generated successfully in {OUTPUT_DIR}!")

if __name__ == "__main__":
    generate_performance_plots()
