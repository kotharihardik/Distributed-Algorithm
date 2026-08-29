# Q7 — Large-Scale Server Log Analytics

Reads a server request log and reports traffic totals, response-time
statistics, status-code breakdown, the busiest 60-second window, and the
top-K servers and endpoints.

Both implementations are deliverables for this section: `log_seq` (one
process) and `log_mpi` (many).

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
src/gen_log.cpp     reproducible log generator (fixed seed)
Makefile            short commands — run "make help"
scripts/run_correctness.sh  verification, P = 1, 2, 4, 8
scripts/run_benchmark.sh    timing sweep, writes results/
scripts/summarize.py        builds the report tables from the raw CSV
scripts/plot_speedup.py     draws the speed-up graph
scripts/job_q7.slurm        SLURM batch job (build + verify + benchmark)
data/               hand-checked logs
results/            generated output — timings, tables, correctness log
commands.txt        every command to run, in order, with comments
```

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
request count, and a tie is broken by the **smaller id**.

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
3. **`S` is read as the number of servers**, ids `0 .. S-1`. A server id
   outside that range is treated as a malformed line and skipped rather than
   written past the end of the table.

## Shortcuts

| | |
|---|---|
| `make` | compile everything into `bin/` |
| `make run` | run the worked example (`make run P=8` to change the process count) |
| `make seq` | the same example through the sequential version |
| `make test` | correctness check at P = 1, 2, 4, 8 |
| `make bench` | timing sweep, writes `results/` |
| `make ctest` | on the cluster: allocate cores, run the correctness check, release |
| `make cbench` | on the cluster: allocate cores, run the benchmark, release |
| `make submit` | hand the whole job to SLURM |
| `make graph` | redraw `results/speedup.png` |
| `make clean` | remove binaries and generated data |

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

## Run

```bash
mpirun -np 4 bin/log_mpi data/sample.txt          # the report on stdout
mpirun -np 8 bin/log_mpi big.txt --time           # plus a timing breakdown
bin/log_seq data/sample.txt                       # sequential version
```

Nothing but the report goes to stdout, so it can be diffed directly. Timing
goes to stderr and only when `--time` is passed.

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

**Reproducible across machines.** The numbers come straight from `mt19937`
rather than from `uniform_int_distribution`, which is not specified by the
standard down to the value it returns — libstdc++ on the cluster and libc++ on
a Mac hand back different sequences from the same seed. Same seed, same file,
on any machine.

## Verification

```bash
make test        # writes results/correctness.txt
```

**44 checks**, all at P = 1, 2, 4, 8:

1. **Hand-checked logs** — the worked example, every request succeeding, every
   request failing, a single line, and a case built so counts tie, which
   forces the "smaller id wins" rule to do some work.
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

| Category | Lines | File | T₁ on the cluster | S(8) | Efficiency at P=8 |
|----------|-------|------|-------------------|------|-------------------|
| Small | 10,000 | 380 KB | 0.0022 s | 4.03× | 0.50 |
| Medium | 100,000 | 3.7 MB | 0.0197 s | 6.69× | 0.84 |
| Large | 1,000,000 | 37 MB | 0.1868 s | 7.04× | 0.88 |
| Very large | 10,000,000 | 369 MB | 1.8806 s | 7.20× | 0.90 |

Powers of ten, a thousand-fold span from end to end.

**Small is deliberately tiny.** At ten thousand lines the whole run takes two
milliseconds, the fixed cost of the reduction is no longer negligible beside
the work, and the speed-up falls well short of P. That row is the contrast
case: it is what makes the communication-vs-computation table worth reading.
Read across the efficiency column and the story is the whole point of the
exercise — 0.50, 0.84, 0.88, 0.90 — parallelism only pays once there is enough
work to hide the fixed costs behind. The communication share says the same
thing from the other side: 17.9% of the run at Small against 1.8% at Very
large.

Above ten million lines the run turns into a disk-read benchmark rather than a
measure of the analytics, which is why the ladder stops there.

Benchmark logs are generated into `data/generated/` on first run and are not
committed — 410 MB, and the seed reproduces them exactly. The small
hand-checked logs in `data/` are committed, since those are the evidence.

## On the cluster

```bash
sbatch scripts/job_q7.slurm      # submit from the Q7 directory
squeue -u $USER
```

See `commands.txt` for the full step-by-step, including the flags this cluster
needs (`openmpi/4.1.5`, `--nodes=1`, `--hint=nomultithread`).

## Implementation notes

**Parsing is done with `strtoll` / `strtod` over an in-memory buffer**, not
with `fscanf` per field. Ten million lines is seventy million numbers, and
`scanf` spends longer parsing them than every tally in the program takes.

**This is the best-scaling of the three questions, and the reason is the
reduction is tiny.** Whatever the size of the log, the merge moves only a few
hundred numbers: eight counters, three response-time values, and three tables
sized by the number of servers, endpoints and 60-second windows — none of
which grows with N. Measured at P = 8, the reduce is under **2.5%** of the run
at every size.

**Reading scales too, and that is the whole point of the byte-range split.**
On Very large the read phase went 0.253 s at P = 1 to 0.040 s at P = 8 — a
factor of 6.4 — because eight processes each pull their own slice off disk at
once. Had rank 0 read the file and scattered it, this phase would have stayed
flat and capped the speed-up near 4×.

**Efficiency is flat once the log is big enough** — 0.84 at a hundred thousand
lines and 0.90 at ten million, barely moving across two orders of magnitude.
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
