# Q4 - Triangle Counting: Results

Graphs: Small = 20000 vertices, 250000 edges; Medium = 10000 vertices, 500000 edges; Large = 6000 vertices, 750000 edges; Very large = 4000 vertices, 1000000 edges.
Timings are the fastest of the repeated runs, in seconds.

## Speed-up $S(P) = T_1 / T_P$

| Input size | $P=1$ | $P=2$ | $P=4$ | $P=8$ |
|------------|:-----:|:-----:|:-----:|:-----:|
| Small | 1.00 | 1.38 | 1.70 | 1.87 |
| Medium | 1.00 | 1.73 | 2.75 | 3.70 |
| Large | 1.00 | 1.83 | 3.30 | 5.35 |
| Very large | 1.00 | 1.89 | 3.56 | 6.36 |

## Runtime (seconds)

| Input size | $P=1$ | $P=2$ | $P=4$ | $P=8$ |
|------------|:-----:|:-----:|:-----:|:-----:|
| Small | 0.0370 | 0.0268 | 0.0217 | 0.0198 |
| Medium | 0.2070 | 0.1198 | 0.0753 | 0.0560 |
| Large | 0.7193 | 0.3923 | 0.2183 | 0.1343 |
| Very large | 1.8489 | 0.9768 | 0.5195 | 0.2906 |

## Efficiency $E(P) = S(P)/P$

| Input size | $P=1$ | $P=2$ | $P=4$ | $P=8$ |
|------------|:-----:|:-----:|:-----:|:-----:|
| Small | 1.00 | 0.69 | 0.43 | 0.23 |
| Medium | 1.00 | 0.86 | 0.69 | 0.46 |
| Large | 1.00 | 0.92 | 0.82 | 0.67 |
| Very large | 1.00 | 0.95 | 0.89 | 0.80 |

## Communication vs. computation

Percentage of each run spent inside MPI calls (broadcasting the edge list, plus the final reduce of the counts).

| Input size | $P=1$ | $P=2$ | $P=4$ | $P=8$ |
|------------|:-----:|:-----:|:-----:|:-----:|
| Small | 0.1% | 6.1% | 12.9% | 21.6% |
| Medium | 0.0% | 2.6% | 9.3% | 14.7% |
| Large | 0.0% | 1.0% | 5.0% | 7.6% |
| Very large | 0.0% | 0.7% | 2.2% | 4.1% |

