# Q7 — Large-Scale Server Log Analytics

Reads a server request log and reports traffic totals, response-time
statistics, status-code breakdown, the busiest 60-second window, and the
top-K servers and endpoints.

Both implementations are deliverables for this section: `log_seq` (one
process) and `log_mpi` (many).

The scripts carry no comments; this file is the documentation for them.

## Quick start

```bash
make                                    # build
make run                                # the paper's example
make test                               # 44 correctness checks at P = 1,2,4,8
make eval LOG=server_log.txt            # a supplied log at P = 1,2,4,8
```

On the cluster, load MPI and ask for cores first:

```bash
module purge && module load openmpi/4.1.5
salloc --nodes=1 --ntasks=8 --hint=nomultithread --time=00:30:00 --partition=debug
```

## The method

**The log is split by bytes, not by lines.** Each process works out which
slice of the file is its own, seeks straight there, and reads only that. With
a log of tens of millions of lines, having rank 0 read it all and scatter it
would make rank 0 the bottleneck and defeat the point.

Byte offsets land mid-line, so each process skips forward to the start of the
next whole line, then keeps reading past its own end until it finishes the
line it was in the middle of. Every line therefore belongs to exactly one
process — none dropped, none counted twice.

After that the work is embarrassingly parallel. Each process tallies its slice
and one round of reductions merges everything:

| Quantity | Reduction |
|---|---|
| counts, byte total, status buckets | `MPI_SUM` |
| response-time sum | `MPI_SUM` |
| response-time min / max | `MPI_MIN` / `MPI_MAX` |
| per-server, per-endpoint, per-interval tables | element-wise `MPI_SUM` |

Endpoint and interval ids have no stated upper bound, so their table sizes
are not known until the data has been seen. Each process notes the range it
saw locally, one `MPI_Allreduce` agrees on the global range, and only then are
the tables sized — which is what lets a single element-wise reduce merge them.

Only rank 0 sorts the top-K lists and prints.

## Layout

```
src/log_mpi.cpp     MPI implementation
src/log_seq.cpp     sequential implementation (also the correctness reference)
src/gen_log.cpp     log generator (fixed seed)

Makefile                    short commands — run "make help"
scripts/run_correctness.sh  verification, P = 1, 2, 4, 8
scripts/run_benchmark.sh    timing sweep over the four sizes, writes results/
scripts/run_eval.sh         one supplied log at P = 1, 2, 4, 8
scripts/job_q7.slurm        SLURM batch job (build + verify + benchmark)

data/               hand-checked logs, with a .expected beside each
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
N K S
timestamp server_id endpoint_id user_id status_code response_time bytes_sent
...                                                        (N such lines)
```

- `N` — number of log lines
- `K` — how many entries to print in each top list
- `S` — number of servers; ids run `0 .. S-1`
- a request is **successful** when its status code is below 400
- the interval a request belongs to is `timestamp / 60`

Output is the block of `KEY value` lines listed in the paper, then
`TOP_SERVERS` and `TOP_ENDPOINTS`. Both top lists are ordered by decreasing
request count, and a tie is broken by the **smaller id**. Nothing but the
report goes to stdout, so it can be diffed directly; timing goes to stderr and
only when `--time` is passed.

Worked example, `data/sample.txt`:

```
6 2 3                                TOTAL_REQUESTS 6
100 0 5 1000 200 12.5 500            SUCCESSFUL_REQUESTS 4
130 1 5 1001 404 30.0 200      →     ...
160 0 7 1002 200 10.0 300            BUSIEST_INTERVAL 2 2
190 2 5 1003 500 45.5 100            TOP_SERVERS
200 0 5 1004 301 20.0 400            0 3 14.166667
250 1 7 1005 200 15.0 250            1 2 22.500000
```

### Choices the paper leaves open

Three things are not pinned down by the question, so they are settled here and
noted for the marker:

1. **Response times print with six decimals**, including `MIN` and `MAX`, so
   the formatting is consistent whether the input is whole numbers or not.
2. **A tie for the busiest interval goes to the earlier one.** Without a rule,
   two runs could disagree.
3. **`S` is read as the number of servers**, ids `0 .. S-1`. A line whose
   server id falls outside that range still counts in the totals, the status
   buckets and the byte total, but is not added to any per-server tally —
   rather than being written past the end of the table.

## Make targets

| | |
|---|---|
| `make` | compile everything into `bin/` |
| `make run` | run the worked example (`make run P=8` to change the process count) |
| `make seq` | the same example through the sequential version |
| `make test` | correctness check at P = 1, 2, 4, 8 |
| `make bench` | timing sweep over the four sizes, writes `results/` |
| `make eval LOG=l.txt` | one supplied log at P = 1, 2, 4, 8 |
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
mpicxx -O2 -std=c++17 -Wall -o bin/log_mpi src/log_mpi.cpp
g++    -O2 -std=c++17 -Wall -o bin/log_seq src/log_seq.cpp
g++    -O2 -std=c++17 -Wall -o bin/gen_log src/gen_log.cpp
```

Clean under `-Wall`, no warnings.

## Run

```bash
mpirun -np 4 bin/log_mpi data/sample.txt          # the report on stdout
mpirun -np 8 bin/log_mpi big.txt --time           # plus a timing breakdown
bin/log_seq data/sample.txt                       # sequential version
```

`--time` adds a per-phase report on stderr, each figure the slowest rank's:

```
procs 8  N 1000000  K 10  S 64
read     0.004446
compute  0.025108
reduce   0.016816
total    0.045235
```

## Running a log supplied on the day

`scripts/run_eval.sh` takes one log file, runs it at P = 1, 2, 4 and 8, and
prints the full report together with the runtime, speed-up, efficiency and MPI
share at each process count.

```bash
make eval LOG=server_log.txt                    # picks up the machine's mpirun flags
bash scripts/run_eval.sh server_log.txt         # or directly
REPS=5 bash scripts/run_eval.sh server_log.txt  # more repeats, fastest kept
PROCS_LIST="1 2 4 8 16" bash scripts/run_eval.sh server_log.txt
```

Output, on a one-million-line log:

```
TOTAL_REQUESTS 1000000
...
 sequential check (bin/log_seq): identical report

Runtime, speed-up and efficiency

  P          time (s)   speed-up   efficiency
  1            0.1020       1.00        1.00
  2            0.0539       1.89        0.95
  4            0.0302       3.38        0.84
  8            0.0452       2.26        0.28

Where the time went (seconds, and MPI as a share of the run)

  P             read     compute      reduce    comm %
  1           0.0109      0.0910      0.0000      0.0%
  8           0.0044      0.0251      0.0168     37.2%
```

Each process count is run three times and the fastest kept (`REPS` changes
that), because one slow run on a shared node says more about the other jobs on
it than about this program.

The correctness check here is stronger than a single number: the **entire
report** must be byte-identical at every process count, and identical to
`log_seq`'s. Since the log is split by byte offset, a boundary landing
mid-line is exactly the bug that only appears once P > 1 — and it would move
`TOTAL_REQUESTS` immediately. Any disagreement is printed as a diff and the
script exits non-zero.

Writes `results/eval_run.txt` and `results/eval_raw.csv`. It never touches the
four files `run_benchmark.sh` produces, so a demo run cannot overwrite the
benchmark results quoted below.

## Generating logs

```bash
bin/gen_log <N> <K> <S> <seed> <outfile> [endpoints]
bin/gen_log 10000000 10 64 12345 data/generated/Verylarge.txt 500
```

The traffic is deliberately lopsided rather than uniform: a fifth of the
servers take about 70% of the requests, the same for endpoints, and timestamps
advance in uneven steps so some 60-second windows are far busier than others.
A uniform log would make the interesting parts of the question meaningless —
every server would get the same share and the top-K lists would just be
whichever ids won a coin toss.

Status codes are roughly 85% 2xx, 5% 3xx, 7% 4xx, 3% 5xx.

The generator is seeded with `srand(seed)` and draws through `rand()`, so the
same seed reproduces the same file on the same machine — verified by matching
MD5 across repeated runs. `rand()` is not specified by the standard down to
the value it returns, so a log built under libc++ on a Mac is not guaranteed
to match one built under libstdc++ on the cluster. That does not affect any
result here: the benchmark logs are regenerated wherever the benchmark runs,
and correctness is always checked against `log_seq` reading the same file.

## Verification

```bash
make test        # writes results/correctness.txt
```

**44 checks**, all at P = 1, 2, 4, 8:

1. **Hand-checked logs** — the worked example, every request succeeding, every
   request failing, a single line, and a case built so counts tie, which
   forces the "smaller id wins" rule to do some work. Each is compared against
   a `data/<name>.expected` worked out by hand.
2. **Generated logs against `log_seq`** — including N not divisible by P, more
   servers than lines, more endpoints than lines, and a three-line log run
   across eight processes.

The strongest signal is that `TOTAL_REQUESTS` always equals `N` exactly. If a
byte boundary ever dropped a line or counted one twice, that number moves
immediately.

## Benchmark

```bash
make bench
```

| File | Contents |
|------|----------|
| `results/benchmark_raw.csv` | every run — read / compute / reduce / total |
| `results/benchmark_log.txt` | readable transcript |
| `results/report_tables.md`  | the four report tables, ready to paste |
| `results/speedup.png`       | speed-up vs. P graph |

| Category | Lines | File | T₁ | S(8) | Efficiency at P=8 |
|----------|-------|------|-----|------|-------------------|
| Small | 10,000 | 378 KB | 0.0021 s | 4.04× | 0.50 |
| Medium | 100,000 | 3.7 MB | 0.0190 s | 6.66× | 0.83 |
| Large | 1,000,000 | 37 MB | 0.1799 s | 7.14× | 0.89 |
| Very large | 10,000,000 | 369 MB | 1.8028 s | 7.33× | 0.92 |

All four use K = 10, S = 64 servers and 500 distinct endpoints, so only the
amount of data changes across the ladder. Powers of ten, a thousand-fold span
from end to end.

**Small is deliberately tiny.** At ten thousand lines the whole run takes two
milliseconds, the fixed cost of the reduction is no longer negligible beside
the work, and the speed-up falls well short of P. That row is the contrast
case: it is what makes the communication-vs-computation table worth reading.
Read across the efficiency column and the story is the whole point of the
exercise — 0.50, 0.83, 0.89, 0.92 — parallelism only pays once there is enough
work to hide the fixed costs behind. The communication share says the same
thing from the other side: 17.9% of the run at Small against 1.8% at Very
large.

Above ten million lines the run turns into a disk-read benchmark rather than a
measure of the analytics, which is why the ladder stops there.

Benchmark logs are generated into `data/generated/` on first run and are not
committed — 410 MB of them, and the seed reproduces them exactly. The small
hand-checked logs in `data/` are committed, since those are the evidence.

### Results

Measured on the cluster, one node, 8 tasks, fastest of three runs.
`results/report_tables.md` has these as pasteable tables.

Speed-up `S(P) = T₁/T_P`:

| Input size | P=1 | P=2 | P=4 | P=8 |
|---|:-:|:-:|:-:|:-:|
| Small | 1.00 | 1.86 | 2.99 | 4.04 |
| Medium | 1.00 | 1.86 | 3.61 | 6.66 |
| Large | 1.00 | 1.96 | 3.82 | 7.14 |
| Very large | 1.00 | 1.96 | 3.87 | 7.33 |

Efficiency `E(P) = S(P)/P`:

| Input size | P=1 | P=2 | P=4 | P=8 |
|---|:-:|:-:|:-:|:-:|
| Small | 1.00 | 0.93 | 0.75 | 0.50 |
| Medium | 1.00 | 0.93 | 0.90 | 0.83 |
| Large | 1.00 | 0.98 | 0.95 | 0.89 |
| Very large | 1.00 | 0.98 | 0.97 | 0.92 |

Share of the run inside MPI calls — the reductions that merge the per-process
tallies. Reading the log is I/O, not communication, and is listed separately
in the raw CSV:

| Input size | P=1 | P=2 | P=4 | P=8 |
|---|:-:|:-:|:-:|:-:|
| Small | 0.1% | 2.4% | 9.8% | 17.9% |
| Medium | 0.0% | 0.8% | 3.4% | 5.4% |
| Large | 0.0% | 0.3% | 0.8% | 1.9% |
| Very large | 0.0% | 0.2% | 1.0% | 1.8% |

## On the cluster

```bash
module purge && module load openmpi/4.1.5
sbatch scripts/job_q7.slurm      # submit from the Q7 directory
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
presented as 48 hyperthreads; without it, 8 tasks land on 4 real cores and the
compute time at P=8 comes out identical to P=4.

The `--mca pml ^ucx --mca osc ^ucx` flags in the job script turn off UCX: it is
advertised on the nodes but `libucp.so.0` is missing, so every rank otherwise
prints a load failure.

## Implementation notes

**Parsing is done with `strtoll` / `strtod` over an in-memory buffer**, not
with `fscanf` per field. Ten million lines is seventy million numbers, and
`scanf` spends longer parsing them than every tally in the program takes.

**This is the best-scaling of the three questions, and the reason is that the
reduction is tiny.** Whatever the size of the log, the merge moves only a few
hundred numbers: eight counters, three response-time values, and three tables
sized by the number of servers, endpoints and 60-second windows — none of
which grows with N. At P = 8 the reduce is 1.9% of the run on Large and 1.8%
on Very large; it only becomes visible on the ten-thousand-line case, where
it reaches 17.9% of a two-millisecond run.

**Reading scales too, and that is the whole point of the byte-range split.**
On Very large the read phase went 0.224 s at P = 1 to 0.036 s at P = 8 — a
factor of 6.3 — because eight processes each pull their own slice off disk at
once. Had rank 0 read the file and scattered it, this phase would have stayed
flat and capped the speed-up near 4×.

**Efficiency is flat once the log is big enough** — 0.83 at a hundred thousand
lines and 0.92 at ten million, barely moving across two orders of magnitude.
There is no replicated setup phase here at all: every process reads its own
data and touches nothing else until the final reduce, so nothing is paid P
times over. Only the ten-thousand-line case falls away (0.50), and that is the
fixed cost of the reduction showing through on a run that lasts two
milliseconds — not a property of the algorithm.

**Where it would stop scaling.** The per-interval table is sized by the *span*
of timestamps divided by 60. A log covering years at one-second resolution
would make that table large enough to matter in the reduce. The per-endpoint
table has the same shape of risk if endpoint ids are sparse but huge — the
table is sized by the largest id, not by how many distinct ids appear.
