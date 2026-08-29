# Q4 — Triangle Counting in an Undirected Graph

Counts every triangle in an undirected graph with MPI. The edges are split
into equal blocks, one per process, and the per-process counts are summed with
`MPI_Reduce`.

## The method

Give each vertex a rank, ordered by degree with the id settling ties:

```
u before v   <=>   (deg[u], u) < (deg[v], v)
```

Point every edge from the earlier vertex to the later one, and write `N+(x)`
for the vertices `x` points at. For a triangle `{a, b, c}` with `a` before `b`
before `c`, the edges come out `a→b`, `a→c`, `b→c`, and `c` appears in `N+(a)`
and `N+(b)` but nowhere else. So

```
triangles = sum over every edge (u,v) of  |N+(u) ∩ N+(v)|
```

finds each triangle exactly once — no halving, no dividing by three, nothing
to undo afterwards.

**That property is what makes the parallel version safe.** A triangle is only
ever counted at one specific edge, so once each process owns a distinct block
of edges, two processes physically cannot count the same triangle. No
double-counting, and no communication during the count.

**Why order by degree rather than by id.** With plain id order, a low-numbered
hub points at nearly all its neighbours and its forward list becomes enormous.
Ordering by degree bounds every forward list at roughly `sqrt(2E)`, which is
what keeps the intersection work near `O(E^1.5)` instead of `O(E·d_max)`.

## Layout

```
src/tri_mpi.cpp     MPI implementation (the deliverable)
src/tri_seq.cpp     sequential reference — the correctness baseline
src/gen_graph.cpp   reproducible random-graph generator (fixed seed)
Makefile                    short commands — run "make help"
scripts/run_correctness.sh  verification, P = 1, 2, 4, 8
scripts/run_benchmark.sh    timing sweep, writes results/
scripts/summarize.py        builds the report tables from the raw CSV
scripts/plot_speedup.py     draws the speed-up graph
scripts/job_q4.slurm        SLURM batch job (build + verify + benchmark)
data/               known-answer graphs
results/            generated output — timings, tables, correctness log
commands.txt        every command to run, in order, with comments
Makefile            short commands - "make help"
```

## Input / output format

Exactly as the question paper specifies.

Input:

```
V E
u v          (E lines, one undirected edge each, vertices 0-indexed)
```

Output: a single integer — the total triangle count. Nothing else goes to
stdout, so it can be diffed directly. Timing goes to stderr, and only when
`--time` is passed.

The worked example from the paper, `data/sample.txt`:

```
4 5           triangles: {0,1,2} and {0,2,3}
0 1
1 2           answer: 2
2 0
2 3
3 0
```

Self-loops and out-of-range endpoints are skipped on read; neither can be part
of a triangle.

## Shortcuts

A Makefile wraps the scripts, so the common things are one word:

| | |
|---|---|
| `make` | compile everything into `bin/` |
| `make run` | run the worked example (`make run P=8` to change the process count) |
| `make seq` | the same example through the sequential reference |
| `make test` | correctness check at P = 1, 2, 4, 8 |
| `make bench` | timing sweep, writes `results/` |
| `make ctest` | on the cluster: allocate cores, run the correctness check, release |
| `make cbench` | on the cluster: allocate cores, run the benchmark, release |
| `make alloc` | just grab a shell with 8 real cores |
| `make submit` | hand the whole job to SLURM |
| `make queue` | `squeue -u $USER` |
| `make graph` | redraw `results/speedup.png` |
| `make clean` | remove binaries and generated data |

`make help` lists them. The launcher differs by machine and the Makefile picks
it automatically — `--bind-to core` on the cluster, `--oversubscribe` on a Mac
(macOS exposes no processor binding, so binding is a hard error there).

## Build

```bash
module load openmpi/4.1.5      # on the cluster
make
```

Which is:

```bash
mpicxx -O2 -std=c++17 -Wall -o bin/tri_mpi   src/tri_mpi.cpp
g++    -O2 -std=c++17 -Wall -o bin/tri_seq   src/tri_seq.cpp
g++    -O2 -std=c++17 -Wall -o bin/gen_graph src/gen_graph.cpp
```

## Run

```bash
mpirun -np 4 bin/tri_mpi data/sample.txt              # prints: 2
mpirun -np 8 bin/tri_mpi graph.txt --time             # plus a timing breakdown
bin/tri_seq data/sample.txt                           # sequential reference
```

## Generating test graphs

```bash
bin/gen_graph <V> <E> <seed> <outfile>
bin/gen_graph 4000 1000000 12345 data/generated/Verylarge.txt
```

A uniform random graph with exactly `E` distinct edges and no self-loops,
`mt19937` driven by an explicit seed — the reproducible generation the
deliverables ask for.

Reproducible **across machines**, which took a fix to achieve. The first
version used `uniform_int_distribution` and `std::shuffle`; neither is
specified by the standard down to the value it returns, so the same seed built
one graph under libc++ on a Mac and a different one under libstdc++ on the
cluster — 21,378,693 triangles against 21,377,463. Only `mt19937`'s raw output
is pinned down, so the bounded draw and the shuffle are now written out by
hand on top of it. Same seed, same file, verified by matching MD5 on both
machines.

## Verification

```bash
bash scripts/run_correctness.sh       # writes results/correctness.txt
```

**56 checks**, all at P = 1, 2, 4, 8:

1. **Known-answer graphs** — the paper's worked example, a lone triangle, a
   path with none, a star with none, two disjoint triangles, and the complete
   graphs K4 and K5. The complete graphs are the sharpest of these: K5 must
   give exactly C(5,3) = 10, and any orientation or double-counting mistake
   changes that number.
2. **Random graphs against `tri_seq`** — sparse, dense, E not divisible by P,
   tiny V, and a graph with mostly isolated vertices. One case is K100, whose
   answer must be exactly C(100,3) = 161,700.

`tri_seq` deliberately shares **nothing** with the parallel version. It keeps
the adjacency as a plain `vector<vector<int>>`, counts common neighbours at
every edge, and divides by three. No orientation, no flattened array. If the
reference were a copy of the thing it is checking, a mistake in the
orientation rule would agree with itself and go unnoticed.

Its speed is irrelevant and never measured: it runs only on the small graphs
above, never on the benchmark graphs, and T₁ in the timing tables is the MPI
program at P = 1, not this.

## Benchmark

```bash
bash scripts/run_benchmark.sh
```

| File | Contents |
|------|----------|
| `results/benchmark_raw.csv` | every run — read / bcast / build / count / reduce / total |
| `results/benchmark_log.txt` | readable transcript |
| `results/report_tables.md`  | the four report tables, ready to paste |
| `results/speedup.png`       | speed-up vs. P graph |

The four graph sizes, all inside the paper's limits of V ≤ 10⁵ and E ≤ 10⁶:

| Category | V | E | File | Triangles | T₁ on the cluster |
|----------|---|---|------|-----------|-------------------|
| Small | 20,000 | 250,000 | 2.6 MB | 2,801 | 0.036 s |
| Medium | 10,000 | 500,000 | 4.7 MB | 177,374 | 0.196 s |
| Large | 6,000 | 750,000 | 6.9 MB | 2,740,263 | 0.677 s |
| Very large | 4,000 | 1,000,000 | 9.0 MB | 21,377,463 | 1.773 s |

Both the edge count and the density climb across the four. That is deliberate:
the work a triangle count actually does is roughly `E²/V`, so a graph with
twice the edges *and* twice the vertices is barely harder. Very large sits at
the paper's ceiling of E = 10⁶.

Benchmark graphs are generated into `data/generated/` on first run and are not
committed — the seed reproduces them exactly. The small known-answer graphs in
`data/` are committed, since those are the correctness evidence.

## On the cluster

```bash
sbatch scripts/job_q4.slurm      # submit from the Q4 directory
squeue -u $USER
```

See `commands.txt` for the full step-by-step, including the flags this cluster
needs (`openmpi/4.1.5`, `--nodes=1`, `--hint=nomultithread`).

## Implementation notes

**The forward adjacency is built without a comparison sort.** The intersection
needs each forward list in increasing order. Rather than sorting every list,
the edges are bucketed by head, and then the lists are filled by tail while
walking that order. The second pass is stable, so the heads inside each list
come out ascending. Two linear passes instead of `E log E`.

**Reading is done by hand, not with `scanf`.** A million edges means two
million integers, and `scanf` spends longer parsing them than the triangle
count itself takes. The file is read in one `fread` and the integers picked out
directly.

**Every process holds the whole edge list.** A triangle sitting on one
process's edge can close through a vertex owned by any other, so the adjacency
has to be globally visible. At the paper's ceiling that is 8 MB per process,
which is nothing, and it buys a counting phase with no communication in it at
all. What gets divided is the *work* — the block of edges each process is
responsible for — and that division is what keeps the count honest.

**The build phase does not parallelise, and it is the scaling limit.** Every
process constructs the same adjacency, so that cost is paid P times over
instead of being divided. The counting phase itself scales almost perfectly —
on Very large it went 1.732 s → 0.219 s from P = 1 to P = 8, a factor of 7.9 —
but the build stays flat at about 0.02 s no matter how many processes there
are, and that fixed cost is what caps the overall figure at 6.33×.

Measured on the cluster, build as a share of the run:

| | P=1 | P=2 | P=4 | P=8 |
|---|:-:|:-:|:-:|:-:|
| Small | 16.0% | 23.8% | 29.2% | 33.4% |
| Very large | 1.1% | 2.6% | 5.0% | 9.7% |

That single table explains the whole efficiency column: the bigger the graph,
the more counting there is to hide the fixed build behind, so efficiency at
P = 8 climbs from 0.24 on Small to 0.81 on Very large. Textbook Amdahl, and
the main thing worth discussing in the performance write-up.

**Load balance is by edge count, not by cost.** The paper asks for equal-sized
edge subsets, so that is what the split does. The cost of an edge depends on
the degrees of its endpoints, so on a graph with a very skewed degree
distribution some processes finish before others. Equal *work* splitting would
need a prefix sum over per-edge costs.

**Counts use `long long`.** A dense graph reaches tens of millions of
triangles — Very large has 21.4 million — and a complete graph on 10⁵ vertices
would overflow 32 bits many times over.
