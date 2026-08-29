# Q4 - Triangle Counting: Results

Graphs: Small = 20000 vertices, 250000 edges; Medium = 10000 vertices, 500000 edges; Large = 6000 vertices, 750000 edges; Very large = 4000 vertices, 1000000 edges.
Timings are the fastest of the repeated runs, in seconds.

## Speed-up $S(P) = T_1 / T_P$

| Input size | $P=1$ | $P=2$ | $P=4$ | $P=8$ |
|------------|:-----:|:-----:|:-----:|:-----:|
| Small | 1.00 | 1.41 | 1.74 | 1.90 |
| Medium | 1.00 | 1.76 | 2.76 | 3.84 |
| Large | 1.00 | 1.86 | 3.40 | 5.46 |
| Very large | 1.00 | 1.94 | 3.65 | 6.49 |

## Runtime (seconds)

| Input size | $P=1$ | $P=2$ | $P=4$ | $P=8$ |
|------------|:-----:|:-----:|:-----:|:-----:|
| Small | 0.0365 | 0.0260 | 0.0210 | 0.0192 |
| Medium | 0.1978 | 0.1127 | 0.0717 | 0.0515 |
| Large | 0.6855 | 0.3684 | 0.2019 | 0.1256 |
| Very large | 1.7793 | 0.9165 | 0.4872 | 0.2742 |

## Efficiency $E(P) = S(P)/P$

| Input size | $P=1$ | $P=2$ | $P=4$ | $P=8$ |
|------------|:-----:|:-----:|:-----:|:-----:|
| Small | 1.00 | 0.70 | 0.43 | 0.24 |
| Medium | 1.00 | 0.88 | 0.69 | 0.48 |
| Large | 1.00 | 0.93 | 0.85 | 0.68 |
| Very large | 1.00 | 0.97 | 0.91 | 0.81 |

## Communication vs. computation

Percentage of each run spent inside MPI calls (broadcasting the edge list, plus the final reduce of the counts).

| Input size | $P=1$ | $P=2$ | $P=4$ | $P=8$ |
|------------|:-----:|:-----:|:-----:|:-----:|
| Small | 0.0% | 6.1% | 11.5% | 19.3% |
| Medium | 0.0% | 2.3% | 9.4% | 13.1% |
| Large | 0.0% | 0.9% | 4.0% | 7.3% |
| Very large | 0.0% | 0.5% | 2.1% | 4.0% |

