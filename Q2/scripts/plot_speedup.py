#!/usr/bin/env python3
"""
Draws the speed-up vs. P graph the report format asks for.

Usage: python3 plot_speedup.py results/benchmark_raw.csv results/speedup.png
"""

import sys

import matplotlib
matplotlib.use("Agg")           # no display on a compute node
import matplotlib.pyplot as plt

from summarize import load, PROCS, ROWS


def main():
    if len(sys.argv) < 3:
        sys.exit("usage: plot_speedup.py <benchmark_raw.csv> <out.png>")

    best, _ = load(sys.argv[1])

    fig, ax = plt.subplots(figsize=(6.5, 4.5))

    for key, name in ROWS:
        base = best.get((key, 1))
        if not base:
            continue
        xs, ys = [], []
        for p in PROCS:
            entry = best.get((key, p))
            if entry:
                xs.append(p)
                ys.append(base["total"] / entry["total"])
        if xs:
            ax.plot(xs, ys, marker="o", label=name)

    ax.plot(PROCS, PROCS, linestyle="--", color="grey",
            linewidth=1, label="ideal")

    ax.set_xlabel("Processes $P$")
    ax.set_ylabel("Speed-up $S(P) = T_1 / T_P$")
    ax.set_title("Q2 Column-Row: speed-up vs. process count")
    ax.set_xticks(PROCS)
    ax.grid(alpha=0.3)
    ax.legend()

    fig.tight_layout()
    fig.savefig(sys.argv[2], dpi=150)
    print("wrote " + sys.argv[2])


if __name__ == "__main__":
    main()
