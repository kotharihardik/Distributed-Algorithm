# Q4 — Triangle Counting in an Undirected Graph

Counts every triangle in an undirected graph with MPI. The edges are split
into equal blocks, one per process, and the per-process counts are summed with
`MPI_Reduce`.

The scripts carry no comments; this file is the documentation for them.

## Quick start

```bash
make                                    # build
make run                                # the paper's example -> 2
make test                               # 56 correctness checks at P = 1,2,4,8
make eval GRAPH=graph.txt               # a supplied graph at P = 1,2,4,8
```

On the cluster, load MPI and ask for cores first:

```bash
module purge && module load openmpi/4.1.5
salloc --nodes=1 --ntasks=8 --hint=nomultithread --time=00:30:00 --partition=debug
```

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
scripts/run_benchmark.sh    timing sweep over the four sizes, writes results/
scripts/run_eval.sh         one supplied graph at P = 1, 2, 4, 8
scripts/job_q4.slurm        SLURM batch job (build + verify + benchmark)

data/               known-answer graphs, with a .expected beside each
results/            timings, report tables, correctness log, speed-up graph
```

The two analysis helpers, `scripts/summarize.py` and `scripts/plot_speedup.py`,
build `results/report_tables.md` and `results/speedup.png` from the raw CSV.
Their **output** is what is submitted, so the `.py` files are excluded by
`.gitignore` and are not in the handed-in tree. `run_benchmark.sh` checks for
them and carries on without them; `run_eval.sh` needs no Python at all.

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

## Make targets

| | |
|---|---|
| `make` | compile everything into `bin/` |
| `make run` | run the worked example (`make run P=8` to change the process count) |
| `make seq` | the same example through the sequential reference |
| `make test` | correctness check at P = 1, 2, 4, 8 |
| `make bench` | timing sweep over the four sizes, writes `results/` |
| `make eval GRAPH=g.txt` | one supplied graph at P = 1, 2, 4, 8 |
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

Clean under `-Wall`, no warnings.

## Run

```bash
mpirun -np 4 bin/tri_mpi data/sample.txt              # prints: 2
mpirun -np 8 bin/tri_mpi graph.txt --time             # plus a timing breakdown
bin/tri_seq data/sample.txt                           # sequential reference
```

`--time` adds a per-phase report on stderr, each figure the slowest rank's:

```
procs 8  V 6000  E 750000
read     0.010027
bcast    0.009266
build    0.030307
count    0.091012
reduce   0.010478
total    0.126718
```

## Running a graph supplied on the day

`scripts/run_eval.sh` takes one graph file, runs it at P = 1, 2, 4 and 8, and
prints the count together with the runtime, speed-up, efficiency and MPI share
at each process count.

```bash
make eval GRAPH=graph.txt                       # picks up the machine's mpirun flags
bash scripts/run_eval.sh graph.txt              # or directly
REPS=5 bash scripts/run_eval.sh graph.txt       # more repeats, fastest kept
PROCS_LIST="1 2 4 8 16" bash scripts/run_eval.sh graph.txt
```

Output, on a 6,000-vertex 750,000-edge graph:

```
 TRIANGLES : 2742714
 sequential check (bin/tri_seq): agrees (2742714)

Runtime, speed-up and efficiency

  P          time (s)   speed-up   efficiency
  1            0.4196       1.00        1.00
  2            0.2152       1.95        0.97
  4            0.1445       2.90        0.73
  8            0.1267       3.31        0.41

Where the time went (seconds, and MPI as a share of the run)

  P           read     bcast     build     count    reduce   comm %
  1         0.0083    0.0000    0.0065    0.4049    0.0000     0.0%
  8         0.0100    0.0093    0.0303    0.0910    0.0105    15.6%
```

Each process count is run three times and the fastest kept (`REPS` changes
that), because one slow run on a shared node says more about the other jobs on
it than about this program. The count is checked for agreement across every P
and against `bin/tri_seq`; if anything disagrees the script says so and exits
non-zero, so a wrong answer cannot pass quietly.

Writes `results/eval_run.txt` and `results/eval_raw.csv`. It never touches the
four files `run_benchmark.sh` produces, so a demo run cannot overwrite the
benchmark results quoted below.

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
make test                             # writes results/correctness.txt
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
make bench
```

| File | Contents |
|------|----------|
| `results/benchmark_raw.csv` | every run — read / bcast / build / count / reduce / total |
| `results/benchmark_log.txt` | readable transcript |
| `results/report_tables.md`  | the four report tables, ready to paste |
| `results/speedup.png`       | speed-up vs. P graph |

The four graph sizes, all inside the paper's limits of V ≤ 10⁵ and E ≤ 10⁶:

| Category | V | E | File | Triangles | T₁ |
|----------|---|---|------|-----------|-----|
| Small | 20,000 | 250,000 | 2.6 MB | 2,801 | 0.036 s |
| Medium | 10,000 | 500,000 | 4.6 MB | 177,374 | 0.196 s |
| Large | 6,000 | 750,000 | 6.9 MB | 2,740,263 | 0.678 s |
| Very large | 4,000 | 1,000,000 | 9.0 MB | 21,377,463 | 1.778 s |

Both the edge count and the density climb across the four. That is deliberate:
the work a triangle count actually does is roughly `E²/V`, so a graph with
twice the edges *and* twice the vertices is barely harder. Very large sits at
the paper's ceiling of E = 10⁶.

Benchmark graphs are generated into `data/generated/` on first run and are not
committed — the seed reproduces them exactly. The small known-answer graphs in
`data/` are committed, since those are the correctness evidence.

### Results

Measured on the cluster, one node, 8 tasks, fastest of three runs.
`results/report_tables.md` has these as pasteable tables.

Speed-up `S(P) = T₁/T_P`:

| Input size | P=1 | P=2 | P=4 | P=8 |
|---|:-:|:-:|:-:|:-:|
| Small | 1.00 | 1.41 | 1.79 | 1.99 |
| Medium | 1.00 | 1.75 | 2.80 | 3.93 |
| Large | 1.00 | 1.88 | 3.37 | 5.53 |
| Very large | 1.00 | 1.93 | 3.65 | 6.47 |

Efficiency `E(P) = S(P)/P`:

| Input size | P=1 | P=2 | P=4 | P=8 |
|---|:-:|:-:|:-:|:-:|
| Small | 1.00 | 0.70 | 0.45 | 0.25 |
| Medium | 1.00 | 0.88 | 0.70 | 0.49 |
| Large | 1.00 | 0.94 | 0.84 | 0.69 |
| Very large | 1.00 | 0.97 | 0.91 | 0.81 |

Share of the run inside MPI calls — the broadcast of the edge list plus the
final reduce:

| Input size | P=1 | P=2 | P=4 | P=8 |
|---|:-:|:-:|:-:|:-:|
| Small | 0.0% | 5.5% | 13.4% | 21.6% |
| Medium | 0.0% | 2.7% | 10.5% | 12.7% |
| Large | 0.0% | 1.3% | 4.8% | 5.9% |
| Very large | 0.0% | 0.6% | 2.5% | 4.0% |

Communication is never the limit here — 4% at the largest size. The limit is
the build phase, below.

## On the cluster

```bash
module purge && module load openmpi/4.1.5
sbatch scripts/job_q4.slurm      # submit from the Q4 directory
squeue -u $USER
```

The job builds, runs the correctness check, then runs the full sweep, writing
`results/slurm_<jobid>.log`. Its `#SBATCH` block asks for one node and 8 tasks
with `--hint=nomultithread`.

**One node with 8 tasks, not 2 nodes × 4.** Each compute node has 40 cores, so
8 ranks fit on one and every rank still gets a core to itself. Multi-node runs
do not work on this cluster — hpcx `mpirun` aborts with `munmap_chunk():
invalid pointer`, `srun` fails in `MPI_Init`, and openmpi/4.1.5 hangs on the
interconnect. Staying on one node sidesteps all of it, and the scaling
measurement is unaffected.

**`--hint=nomultithread` is not optional.** The nodes are 24 real cores
presented as 48 hyperthreads; without it, 8 tasks land on 4 real cores and P=8
comes out roughly half as fast, silently.

The `--mca pml ^ucx --mca osc ^ucx` flags in the job script turn off UCX: it is
advertised on the nodes but `libucp.so.0` is missing, so every rank otherwise
prints a load failure.

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
on Very large it went 1.740 s → 0.222 s from P = 1 to P = 8, a factor of 7.9 —
but the build stays flat at about 0.02 s no matter how many processes there
are, and that fixed cost is what caps the overall figure at 6.47×.

Build as a share of the run:

| | P=1 | P=2 | P=4 | P=8 |
|---|:-:|:-:|:-:|:-:|
| Small | 15.4% | 23.5% | 30.1% | 34.1% |
| Very large | 1.2% | 2.6% | 4.9% | 10.0% |

That single table explains the whole efficiency column: the bigger the graph,
the more counting there is to hide the fixed build behind, so efficiency at
P = 8 climbs from 0.25 on Small to 0.81 on Very large. Textbook Amdahl, and
the main thing worth discussing in the performance write-up.

**Load balance is by edge count, not by cost.** The paper asks for equal-sized
edge subsets, so that is what the split does. The cost of an edge depends on
the degrees of its endpoints, so on a graph with a very skewed degree
distribution some processes finish before others. Equal *work* splitting would
need a prefix sum over per-edge costs.

**Counts use `long long`.** A dense graph reaches tens of millions of
triangles — Very large has 21.4 million — and a complete graph on 10⁵ vertices
would overflow 32 bits many times over.
