# Q2 — Matrix Multiplication, Column-Row Method

Multiplies two integer matrices with MPI. The shared inner dimension `n` is
split across the processes: each one takes a block of columns of `A` with the
matching block of rows of `B`, forms the outer products, and a single
`MPI_Reduce` sums the partial results onto rank 0.

Both implementations are deliverables: `matmul_seq` (one process) and
`matmul_mpi` (many).

## Quick start

```bash
make                                    # build
make run                                # the paper's example
make test                               # 52 correctness checks at P = 1,2,4,8
make eval INPUT=input.txt               # a supplied input at P = 1,2,4,8
```

On the cluster, load MPI and ask for cores first:

```bash
module purge && module load openmpi/4.1.5
salloc --nodes=1 --ntasks=8 --hint=nomultithread --time=00:30:00 --partition=debug
```

## The method

Write `A` as columns and `B` as rows. Then

```
C = A x B = sum over k of  (column k of A) x (row k of B)
```

Each term is an outer product: a full `m x p` matrix. The sum over `k` is what
gets divided. Process `r` receives `n/nprocs` of the `(column, row)` pairs —
the remainder spread one each over the first few ranks, so an `n` that does
not divide evenly is handled without a special case — builds the partial sum
of its own outer products, and one reduce adds the partials together.

| Phase | What happens |
|---|---|
| `scatter` | two `MPI_Scatterv` calls send each process its columns of `A` and rows of `B` |
| `compute` | the outer products accumulate into a local `m x p` buffer |
| `reduce` | one `MPI_Reduce` with `MPI_SUM` merges the partials onto rank 0 |

**Every process holds a full `m x p` accumulator.** That is what makes the
communication a single reduce with no data-dependent structure, and it is also
the method's cost: the reduce moves `m x p` values regardless of how many
processes there are. Rank 0 sorts nothing and writes the result directly.

A zero entry in `A` skips its inner loop, so a sparse column costs nothing.

Products accumulate in `long long` — a 4000×4000 product of entries up to 9 in
magnitude reaches about 3.2×10⁵ per element, but the headroom costs nothing
and removes the question.

## Layout

```
src/matmul_mpi.cpp     MPI implementation
src/matmul_seq.cpp     sequential implementation (also the correctness reference)
src/gen_matrix.cpp     matrix generator (fixed seed)

Makefile                    short commands — run "make help"
scripts/run_correctness.sh  verification, P = 1, 2, 4, 8
scripts/run_benchmark.sh    timing sweep over the five sizes, writes results/
scripts/run_eval.sh         one supplied input at P = 1, 2, 4, 8
scripts/job_q2.slurm        SLURM batch job (build + verify + benchmark)

data/               worked examples and edge cases, with a .expected beside each
results/            timings, report tables, correctness log, speed-up graph
```

The two analysis helpers, `scripts/summarize.py` and `scripts/plot_speedup.py`,
build `results/report_tables.md` and `results/speedup.png` from the raw CSV.
Their **output** is what is submitted, so the `.py` files are excluded by
`.gitignore` and are not in the handed-in tree. `run_benchmark.sh` checks for
them and carries on without them; `run_eval.sh` needs no Python at all.

## Input / output format

Input, exactly as the question paper specifies:

```
m n p
                 (m rows of n values each — the matrix A)
                 (n rows of p values each — the matrix B)
```

Output is the `m x p` product, one row per line, values separated by single
spaces. It goes to stdout, or to a file with `-o`.

Worked example, `data/example1.txt`:

```
3 2 3            A = 3x2      B = 2x3          C = 3x3
1 2                                            4  3  2
0 3        →                              →    3  0 -3
-1 4                                           2 -3 -8
2 3 4
1 0 -1
```

The edge cases in `data/` cover a single shared dimension (`n = 1`), a single
row (`m = 1`) and a single column (`p = 1`) — the shapes where a block split
most easily goes wrong.

## Make targets

| | |
|---|---|
| `make` | compile everything into `bin/` |
| `make run` | run the worked example (`make run P=8` to change the process count) |
| `make seq` | the same example through the sequential version |
| `make test` | correctness check at P = 1, 2, 4, 8 |
| `make bench` | timing sweep over the five sizes, writes `results/` |
| `make eval INPUT=i.txt` | one supplied input at P = 1, 2, 4, 8 |
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
mpicxx -O2 -std=c++17 -Wall -o bin/matmul_mpi src/matmul_mpi.cpp
g++    -O2 -std=c++17 -Wall -o bin/matmul_seq src/matmul_seq.cpp
g++    -O2 -std=c++17 -Wall -o bin/gen_matrix src/gen_matrix.cpp
```

Clean under `-Wall`, no warnings.

## Run

```bash
mpirun -np 4 bin/matmul_mpi data/example1.txt        # product on stdout
mpirun -np 8 bin/matmul_mpi big.txt -o C.txt --time  # to a file, plus timings
bin/matmul_seq data/example1.txt                     # sequential version
```

`--time` adds a per-phase report on stderr, each figure the slowest rank's:

```
procs 8  m 4000  n 4000  p 4000
scatter  0.020033
compute  7.988371
reduce   0.194935
total    8.141338
```

## Running an input supplied on the day

`scripts/run_eval.sh` takes one input file, runs it at P = 1, 2, 4 and 8, and
prints the product together with the runtime, speed-up, efficiency and MPI
share at each process count.

```bash
make eval INPUT=input.txt                    # picks up the machine's mpirun flags
bash scripts/run_eval.sh input.txt           # or directly
REPS=5 bash scripts/run_eval.sh input.txt    # more repeats, fastest kept
PROCS_LIST="1 2 4 8 16" bash scripts/run_eval.sh input.txt
```

A product of 20 rows or fewer is printed in full; anything larger is written
to `results/eval_output.txt` and only its dimensions are shown.

```
 PRODUCT : 900 x 900
 sequential check (bin/matmul_seq): identical product

Runtime, speed-up and efficiency

  P          time (s)   speed-up   efficiency
  1            0.1104       1.00        1.00
  2            0.0622       1.78        0.89
  ...
```

Each process count is run three times and the fastest kept (`REPS` changes
that), because one slow run on a shared node says more about the other jobs on
it than about this program.

The product must be identical at every process count and identical to
`matmul_seq`'s — compared with whitespace normalised, so spacing differences
never masquerade as a wrong answer. Any disagreement is reported and the
script exits non-zero.

Writes `results/eval_run.txt`, `results/eval_raw.csv` and
`results/eval_output.txt`. It never touches the four files `run_benchmark.sh`
produces, so a demo run cannot overwrite the benchmark results quoted below.

## Generating matrices

```bash
bin/gen_matrix <m> <n> <p> <seed> <outfile> [maxabs]
bin/gen_matrix 4000 4000 4000 12345 data/generated/Verylarge.txt
```

Writes both matrices into one file in the input format above. Entries are
drawn from `-maxabs .. maxabs`, default 9.

**Delete `data/generated/` before changing the benchmark sizes.**
`run_benchmark.sh` only generates a file when it is not already there, so a
leftover matrix from an earlier size list is silently reused and timed under
the new label. `make clean` removes the directory.

## Verification

```bash
make test        # writes results/correctness.txt
```

**52 checks**, all at P = 1, 2, 4, 8:

1. **Supplied examples and edge cases** — the two worked examples from the
   paper, plus `n = 1`, `m = 1` and `p = 1`, each compared against a
   `data/<name>.expected`.
2. **Random matrices against `matmul_seq`** — eight shapes including `n` not
   divisible by P (100×63×80), very wide and very tall cases (400×20×30,
   20×400×30), and each of the three degenerate dimensions again at a larger
   size.

Comparison normalises whitespace before diffing, so only the numbers have to
match, not the spacing.

## Benchmark

```bash
make bench
```

| File | Contents |
|------|----------|
| `results/benchmark_raw.csv` | every run — scatter / compute / reduce / total |
| `results/benchmark_log.txt` | readable transcript |
| `results/report_tables.md`  | the four report tables, ready to paste |
| `results/speedup.png`       | speed-up vs. P graph |

| Category | A × B | T₁ | S(8) | Efficiency at P=8 |
|----------|-------|-----|------|-------------------|
| Tiny | 100×100 × 100×100 | 0.0006 s | 0.99× | 0.12 |
| Small | 500×500 × 500×500 | 0.0695 s | 5.03× | 0.63 |
| Medium | 1000×1000 × 1000×1000 | 0.5412 s | 4.54× | 0.57 |
| Large | 2000×2000 × 2000×2000 | 5.6950 s | 5.55× | 0.69 |
| Very large | 4000×4000 × 4000×4000 | 47.9148 s | 5.89× | 0.74 |

**Tiny is deliberately included as the contrast case.** At 100×100 the whole
run takes six hundred microseconds, and by P = 8 the scatter and reduce
together are **90%** of it — the speed-up is 0.99×, meaning eight processes
finish no sooner than one. That row is what makes the
communication-vs-computation table worth reading.

From Small upwards the trend is the expected one: efficiency at P = 8 climbs
0.63 → 0.69 → 0.74 as the matrices grow, because the compute term is `O(mnp)`
while the reduce only grows as `O(mp)`. Medium's 0.57 sits below that line —
its P = 8 run gained only 1.29× over P = 4 where the neighbouring sizes gained
about 1.6×, which reads as one unlucky measurement on a shared node rather
than a property of the method.

Benchmark matrices are generated into `data/generated/` on first run and are
not committed — the seed reproduces them exactly. The small examples in
`data/` are committed, since those are the correctness evidence.

### Results

Measured on the cluster, one node, 8 tasks, fastest of three runs.
`results/report_tables.md` has these as pasteable tables.

Speed-up `S(P) = T₁/T_P`:

| Input size | P=1 | P=2 | P=4 | P=8 |
|---|:-:|:-:|:-:|:-:|
| Tiny | 1.00 | 1.35 | 1.29 | 0.99 |
| Small | 1.00 | 1.89 | 3.13 | 5.03 |
| Medium | 1.00 | 1.89 | 3.52 | 4.54 |
| Large | 1.00 | 1.78 | 3.46 | 5.55 |
| Very large | 1.00 | 1.80 | 3.58 | 5.89 |

Efficiency `E(P) = S(P)/P`:

| Input size | P=1 | P=2 | P=4 | P=8 |
|---|:-:|:-:|:-:|:-:|
| Tiny | 1.00 | 0.68 | 0.32 | 0.12 |
| Small | 1.00 | 0.94 | 0.78 | 0.63 |
| Medium | 1.00 | 0.95 | 0.88 | 0.57 |
| Large | 1.00 | 0.89 | 0.86 | 0.69 |
| Very large | 1.00 | 0.90 | 0.89 | 0.74 |

Share of the run inside MPI calls — the two scatters plus the final reduce:

| Input size | P=1 | P=2 | P=4 | P=8 |
|---|:-:|:-:|:-:|:-:|
| Tiny | 5.6% | 33.0% | 65.2% | 90.0% |
| Small | 1.3% | 5.8% | 20.5% | 33.3% |
| Medium | 0.5% | 2.0% | 7.7% | 15.2% |
| Large | 0.2% | 0.6% | 3.1% | 4.7% |
| Very large | 0.1% | 0.4% | 1.3% | 2.6% |

## On the cluster

```bash
module purge && module load openmpi/4.1.5
sbatch scripts/job_q2.slurm      # submit from the Q2 directory
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

**The split is over `n`, the shared dimension — not over rows of `C`.** That
is what the column-row method means, and it is the reason the merge is a
single `MPI_Reduce` rather than a gather: every process computes a partial
version of the *whole* output, and the partials add.

**The scatter is cheap and stays cheap.** It moves `n(m + p)` values once,
against `mnp` multiply-adds in the compute phase, so its share falls as the
matrices grow — 0.02 s of a 47.9 s run at Very large.

**The reduce is what limits the scaling.** It moves `m x p` values no matter
how many processes there are, so as P rises it becomes a larger fraction of a
shrinking runtime: 0.04 s of 47.9 s at P = 1 against 0.19 s of 8.1 s at P = 8
on Very large. This is the structural cost of every process holding a full
accumulator, and it is why efficiency at P = 8 tops out near 0.74 rather than
approaching 1.

**An uneven `n` needs no special case.** The remainder is spread one pair each
over the first `n % nprocs` ranks, so the block sizes differ by at most one and
`MPI_Scatterv` handles the uneven counts directly. `100×63×80` in the
correctness suite exercises exactly this at P = 4 and P = 8.
