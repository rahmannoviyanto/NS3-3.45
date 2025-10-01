#!/usr/bin/env python3
import pandas as pd
import matplotlib.pyplot as plt

def main():
    # Baca CSV
    df = pd.read_csv("flow_timeseries.csv")
    df["Time"] = df["Time"].astype(int)

    # Setup figure
    fig, axes = plt.subplots(4, 1, figsize=(10, 12), sharex=True)
    fig.subplots_adjust(hspace=0.3)

    # Tentukan range waktu
    min_t, max_t = df["Time"].min(), df["Time"].max()
    xticks = range(min_t, max_t + 1)

    # 1. Throughput
    for flow, data in df.groupby("Flow"):
        axes[0].plot(data["Time"], data["Throughput(Mbps)"], marker="o", label=flow)
    axes[0].set_ylabel("Throughput (Mbps)")
    axes[0].legend()
    axes[0].grid(True)
    axes[0].set_xticks(xticks)

    # 2. PDR
    for flow, data in df.groupby("Flow"):
        axes[1].plot(data["Time"], data["PDR(%)"], marker="s", label=flow)
    axes[1].set_ylabel("PDR (%)")
    axes[1].grid(True)
    axes[1].set_xticks(xticks)

    # 3. Loss
    for flow, data in df.groupby("Flow"):
        axes[2].plot(data["Time"], data["Loss(%)"], marker="^", label=flow)
    axes[2].set_ylabel("Loss (%)")
    axes[2].grid(True)
    axes[2].set_xticks(xticks)

    # 4. Delay
    for flow, data in df.groupby("Flow"):
        axes[3].plot(data["Time"], data["Delay(ms)"], marker="d", label=flow)
    axes[3].set_ylabel("Delay (ms)")
    axes[3].set_xlabel("Time (s)")
    axes[3].grid(True)
    axes[3].set_xticks(xticks)

    # Simpan ke file PNG
    plt.savefig("metrics.png", dpi=300)
    print("✅ Grafik berhasil disimpan ke metrics.png")

if __name__ == "__main__":
    main()
