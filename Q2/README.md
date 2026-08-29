# Q2 — Distributed Matrix Multiplication (Column-Row Method)

C = A × B for A (m×n) and B (n×p), computed as the sum of n rank-1 outer
products:

```
C = sum over k = 1..n of  A[:,k] · B[k,:]
```

Column *k* of A pairs with row *k* of B. The n pairs are divided among the
processes; each process sums its own pairs into a local m×p matrix, and the
locals are added together with `MPI_Reduce` / `MPI_SUM`.

Unlike the Row-Row method, nothing is broadcast in full and the collection step
is a **sum**, not a gather.

## Layout

```
src/matmul_mpi.cpp    MPI implementation (the deliverable)
src/matmul_seq.cpp    sequential reference — correctness + single-core timing
src/gen_matrix.cpp    reproducible test-data generator (fixed seed)
Makefile                      short commands — run "make help"
scripts/run_correctness.sh    verification, P = 1, 2, 4, 8
scripts/run_benchmark.sh      timing sweep, writes results/
scripts/summarize.py          builds the report tables from the raw CSV
scripts/job_q2.slurm          SLURM batch job (build + verify + benchmark)
data/                 worked examples from the question paper + edge cases
results/              generated output — timings, tables, correctness log
```

## Input / output format

Input file:

```
m n p
<m lines of n integers>     A
<n lines of p integers>     B
```

Output: m lines of p integers — the matrix C. Nothing else is printed on
stdout, so the output can be diffed directly. Timing goes to stderr and only
when `--time` is passed.

Example (`data/example1.txt`, worked through in the question paper):

```
3 2 3          A = [ 1  2]      B = [2  3  4]
1 2                [ 0  3]          [1  0 -1]
0 3                [-1  4]
-1 4
2 3 4          C = [4  3  2]
1 0 -1             [3  0 -3]
                   [2 -3 -8]
```

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
module load hpcx-2.7.0/hpcx-ompi     # on the cluster
make
```

Which is just:

```bash
mpicxx -O2 -std=c++17 -o bin/matmul_mpi src/matmul_mpi.cpp
g++    -O2 -std=c++17 -o bin/matmul_seq src/matmul_seq.cpp
g++    -O2 -std=c++17 -o bin/gen_matrix src/gen_matrix.cpp
```

## Run

```bash
mpirun -np 4 bin/matmul_mpi data/example1.txt              # C to stdout
mpirun -np 4 bin/matmul_mpi data/example1.txt -o C.txt     # C to a file
mpirun -np 4 bin/matmul_mpi data/large.txt -o /dev/null --time   # timings
```

Sequential reference:

```bash
bin/matmul_seq data/example1.txt -o C_ref.txt
```

## Generating test data

`mt19937` with an explicit seed, so a given command always produces the same
file:

```bash
bin/gen_matrix <m> <n> <p> <seed> <outfile> [maxabs]
bin/gen_matrix 1000 1000 1000 12345 data/generated/large.txt
```

Entries are drawn uniformly from −9..9 (`maxabs` defaults to 9). The benchmark
regenerates its own inputs, which is why `data/generated/` is not committed.

## Verification

```bash
bash scripts/run_correctness.sh        # writes results/correctness.txt
```

Two layers of checking:

1. The two worked examples from the question paper, plus the edge cases the
   constraints call out (n = 1, m = 1, p = 1), each against a hand-computed
   expected file.
2. Random matrices compared against `matmul_seq`, over shapes that cover
   square, n divisible by P, n **not** divisible by P, tall (m ≫ n), wide
   (n ≫ m), and single-row / single-column inputs.

Everything runs at P = 1, 2, 4, 8.

## Benchmark

```bash
bash scripts/run_benchmark.sh
```

Sweeps six input sizes against P = 1, 2, 4, 8, repeating each run three times
and keeping the fastest (a shared cluster produces the odd slow run). Output:

| File | Contents |
|------|----------|
| `results/benchmark_raw.csv` | every individual run — scatter / compute / reduce / total |
| `results/benchmark_log.txt` | readable transcript of the sweep |
| `results/report_tables.md`  | the four report tables, ready to paste |
| `results/speedup.png`       | speed-up vs. P graph |

`REPS` and `SEED` can be overridden: `REPS=5 bash scripts/run_benchmark.sh`.

The four sizes are exactly the categories the report format asks for. Small and
Medium have n **not** divisible by 4 or 8, Large and Very large divide evenly,
so both split cases are covered at realistic sizes:

| Category | A | B | vs. the paper's "1000×1000 or more" |
|----------|---|---|------------------------------------|
| Small | 500×500 | 500×500 | below — the contrast case |
| Medium | 1000×1000 | 1000×1000 | exactly the paper's figure |
| Large | 2000×2000 | 2000×2000 | 2× |
| Very large | 4000×4000 | 4000×4000 | 4× |

The paper puts the timing range at "1000 × 1000 or more", so Medium sits on
that figure and the two larger categories go above it. Small is deliberately
below the threshold: at n = 500 the run finishes before MPI has finished
starting up, so its speed-up stays near 1. That is not a failure — it is the
row that makes the communication-vs-computation table mean something.

The paper's other size, 3×3 "for manual verification", is not a timing
category at all; it is covered by the worked examples in `data/`.

The paper's "n divisible by P / not divisible by P" requirement is a
correctness property, so it is tested where it belongs — `run_correctness.sh`
runs 100×64×80 (divisible) against 100×63×80 (not) at every process count.
Small also happens to be indivisible by 4 and 8 at full scale.

Measured cost of the largest case: 75 MB input, 3.3 s to generate, 122 MB of
accumulator per rank.

Benchmark inputs are **generated on the fly** into `data/generated/`, not stored
in the repo — the seed makes them reproducible, so committing several hundred MB
of text would be pointless. The files that *are* committed in `data/` are the
small correctness cases, which is a separate job (see Verification above).

## On the cluster

```bash
sbatch scripts/job_q2.slurm      # submit from the Q2 directory
squeue -u $USER
```

The job asks for 2 nodes × 4 tasks = 8 slots, then builds, verifies and
benchmarks in one go. Everything lands in `results/`.

## Implementation notes

**A is stored transposed.** The master reads A into an n×m buffer, so column
*k* occupies one contiguous block. `MPI_Scatterv` can then send a process its
columns directly, with no packing step. B is already row-major, so its rows are
contiguous as they are.

**Uneven split.** With n pairs over P processes, the first `n % P` processes
take one extra pair. If P > n the surplus processes simply receive zero pairs,
contribute a zero matrix to the reduce, and the answer is still correct — this
is checked in the simulation for P = 16 against n as small as 1.

**Accumulation in `long long`.** Entries are integers, but m·p products with n
terms overflow 32 bits well before the large test sizes, so the local matrix
and the reduce both use `MPI_LONG_LONG`.

**The kernel is blocked, and it matters enormously.** The obvious loop order
(k outermost, then i, then j) sweeps the whole of C once per k. That works out
to one multiply-add per 16 bytes of read-modify-write traffic on C, so the run
is limited by memory bandwidth rather than by arithmetic — and because every
process shares the same memory bus, adding processes then buys nothing at all.
Measured on an 8-core machine at n = 2000, the unblocked version gave S(8) =
0.80: *slower* with eight processes than with one, with `compute` time flat
across P.

Tiling C into 64×256 blocks and running every k through a resident tile drops
C's traffic from once-per-k to once-per-tile-pass. Same measurement afterwards:
S(8) = 3.11, with compute time falling 1.22 s → 0.38 s as P goes 1 → 8. The
sequential reference uses the identical kernel so the comparison stays fair.
Zero entries of A still skip a strip of work.

**Memory cost.** This is the method's real weakness: *every* process holds a
full m×p accumulator, so total memory is P·m·p rather than m·p. At 2000×2000
with 8 processes that is 8 × 32 MB. The Row-Row method does not pay this.

**Communication cost.** Scatter moves n·(m+p) integers in total — each process
only gets what it needs. The reduce moves m·p values from every process and is
independent of n, so as P grows the reduce comes to dominate. That is the
scaling limit visible in the efficiency table.
