# Distributed Systems — Home Work 2

MPI implementations for the three assigned questions, one from each section of
the question paper.

| Section | Question | Topic | Status |
|---------|----------|-------|--------|
| 1. Distributed Algorithms | **Q2** | Matrix multiplication, Column-Row method | implemented |
| 2. Graph Algorithms | **Q4** | Triangle counting in an undirected graph | implemented |
| 3. Real-World Applications | **Q7** | Large-scale server log analytics | implemented |

## Layout

```
Q2/                  Matrix multiplication, Column-Row — see its README
Q4/                  Triangle counting
Q7/                  Server log analytics
docs/                question paper, report format, SLURM reference material
CLUSTER_NOTES.md     how to reach the cluster and the SLURM commands
.env                 cluster credentials — gitignored, never committed
```

Each question directory follows the same shape:

```
src/       source
scripts/   build, correctness, benchmark, SLURM job
data/      sample inputs and expected outputs
results/   generated timings and tables
```

## Quick start

```bash
module purge && module load openmpi/4.1.5     # on the cluster
make                 # build both questions
make test            # correctness, both
cd Q2 && make submit # or hand one to SLURM
```

`make help` at the top level, or `cd Q4 && make help` for the per-question
targets.

## Deliverables checklist

From the question paper, for each of Q2 and Q4:

- [x] MPI source for the assigned algorithm
- [x] Runs correctly at P = 1, 2, 4, 8
- [x] README with compilation and execution instructions
- [x] Correctness verification against a sequential computation
- [x] Execution times for P = 1, 2, 4, 8 over varying input sizes
- [x] Speed-up / efficiency plots and analysis of communication vs computation

All ticked items are done for **Q2, Q4 and Q7**, measured on the IIIT RCE
cluster. Q7's section additionally requires a sequential implementation, a
dataset generator and a documented generation procedure — all three are in
`Q7/src/`.

## Results so far

Speed-up at P = 8 on the largest input:

| | Best S(8) | Efficiency | Correctness |
|---|---|---|---|
| Q2 — Column-Row matrix multiply | 7.68× | 0.96 | 52/52 |
| Q4 — Triangle counting | 6.36× | 0.80 | 56/56 |
| Q7 — Server log analytics | 7.20× | 0.90 | 44/44 |

Cluster notes that apply to both: use `openmpi/4.1.5` (the hpcx module
crashes), `--nodes=1` (multi-node MPI does not work here), and
`--hint=nomultithread` (the nodes are 24 cores presented as 48 threads, and
without it eight ranks land on four real cores).
