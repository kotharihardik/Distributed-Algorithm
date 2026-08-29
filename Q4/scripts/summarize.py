#!/usr/bin/env python3
"""
Builds the report tables from results/benchmark_raw.csv.

Produces exactly what docs/report_format.md asks for and nothing else:
  - Speed-up      S(P) = T1 / TP        (the required table)
  - Runtime       raw seconds
  - Efficiency    E(P) = S(P) / P
  - Communication vs computation, as % of the run spent in MPI calls

For Q4 the MPI calls are the broadcast of the edge list and the final reduce
of the per-process counts.

Usage: python3 summarize.py results/benchmark_raw.csv > results/report_tables.md
"""

import csv
import sys

PROCS = [1, 2, 4, 8]

# csv label -> the row name used in the report
ROWS = [
    ("Small", "Small"),
    ("Medium", "Medium"),
    ("Large", "Large"),
    ("Verylarge", "Very large"),
]


def num(text):
    """Blank or unparseable timings count as zero rather than killing the run."""
    try:
        return float(text)
    except (TypeError, ValueError):
        return 0.0


def load(path):
    """Keep the fastest repeat for each (label, procs) pair."""
    best = {}
    shape = {}

    with open(path) as fh:
        for row in csv.DictReader(fh):
            label = row["label"]
            procs = int(row["procs"])
            total = num(row["total"])
            if total <= 0:
                continue

            shape[label] = (row["V"], row["E"])
            key = (label, procs)

            if key not in best or total < best[key]["total"]:
                best[key] = {
                    "total": total,
                    "bcast": num(row["bcast"]),
                    "build": num(row["build"]),
                    "count": num(row["count"]),
                    "reduce": num(row["reduce"]),
                }

    return best, shape


HEADER = "| Input size | " + " | ".join("$P={}$".format(p) for p in PROCS) + " |"
DIVIDER = "|------------|" + ":-----:|" * len(PROCS)


def table(title, best, value_fn, note=None):
    print("## {}\n".format(title))
    if note:
        print(note + "\n")
    print(HEADER)
    print(DIVIDER)

    for key, name in ROWS:
        cells = []
        base = best.get((key, 1))
        for p in PROCS:
            entry = best.get((key, p))
            cells.append(value_fn(base, entry, p) if entry else "")
        print("| {} | {} |".format(name, " | ".join(cells)))
    print()


def main():
    if len(sys.argv) < 2:
        sys.exit("usage: summarize.py <benchmark_raw.csv>")

    best, shape = load(sys.argv[1])

    print("# Q4 - Triangle Counting: Results\n")

    sizes = []
    for key, name in ROWS:
        if key in shape:
            V, E = shape[key]
            sizes.append("{} = {} vertices, {} edges".format(name, V, E))
    if sizes:
        print("Graphs: " + "; ".join(sizes) + ".")
    print("Timings are the fastest of the repeated runs, in seconds.\n")

    table("Speed-up $S(P) = T_1 / T_P$", best,
          lambda base, e, p: "{:.2f}".format(base["total"] / e["total"]) if base else "")

    table("Runtime (seconds)", best,
          lambda base, e, p: "{:.4f}".format(e["total"]))

    table("Efficiency $E(P) = S(P)/P$", best,
          lambda base, e, p: "{:.2f}".format(base["total"] / e["total"] / p) if base else "")

    table("Communication vs. computation", best,
          lambda base, e, p: "{:.1f}%".format(
              100.0 * (e["bcast"] + e["reduce"]) / e["total"]) if e["total"] > 0 else "",
          note="Percentage of each run spent inside MPI calls "
               "(broadcasting the edge list, plus the final reduce of the counts).")


if __name__ == "__main__":
    main()
