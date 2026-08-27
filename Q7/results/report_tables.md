# Q7 - Server Log Analytics: Results

Logs: Small = 10000 log lines, 64 servers; Medium = 100000 log lines, 64 servers; Large = 1000000 log lines, 64 servers; Very large = 10000000 log lines, 64 servers.
Timings are the fastest of the repeated runs, in seconds.

## Speed-up $S(P) = T_1 / T_P$

| Input size | $P=1$ | $P=2$ | $P=4$ | $P=8$ |
|------------|:-----:|:-----:|:-----:|:-----:|
| Small | 1.00 | 1.86 | 2.99 | 4.04 |
| Medium | 1.00 | 1.86 | 3.61 | 6.66 |
| Large | 1.00 | 1.96 | 3.82 | 7.14 |
| Very large | 1.00 | 1.96 | 3.87 | 7.33 |

## Runtime (seconds)

| Input size | $P=1$ | $P=2$ | $P=4$ | $P=8$ |
|------------|:-----:|:-----:|:-----:|:-----:|
| Small | 0.0021 | 0.0011 | 0.0007 | 0.0005 |
| Medium | 0.0190 | 0.0102 | 0.0053 | 0.0029 |
| Large | 0.1799 | 0.0917 | 0.0471 | 0.0252 |
| Very large | 1.8028 | 0.9193 | 0.4658 | 0.2460 |

## Efficiency $E(P) = S(P)/P$

| Input size | $P=1$ | $P=2$ | $P=4$ | $P=8$ |
|------------|:-----:|:-----:|:-----:|:-----:|
| Small | 1.00 | 0.93 | 0.75 | 0.50 |
| Medium | 1.00 | 0.93 | 0.90 | 0.83 |
| Large | 1.00 | 0.98 | 0.95 | 0.89 |
| Very large | 1.00 | 0.98 | 0.97 | 0.92 |

## Communication vs. computation

Percentage of each run spent inside MPI calls - the reductions that merge the per-process tallies. Reading the log is I/O, not communication, and is listed separately in benchmark_raw.csv.

| Input size | $P=1$ | $P=2$ | $P=4$ | $P=8$ |
|------------|:-----:|:-----:|:-----:|:-----:|
| Small | 0.1% | 2.4% | 9.8% | 17.9% |
| Medium | 0.0% | 0.8% | 3.4% | 5.4% |
| Large | 0.0% | 0.3% | 0.8% | 1.9% |
| Very large | 0.0% | 0.2% | 1.0% | 1.8% |

