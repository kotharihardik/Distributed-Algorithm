# Q4 - Triangle Counting: Results

Graphs: Small = 20000 vertices, 250000 edges; Medium = 10000 vertices, 500000 edges; Large = 6000 vertices, 750000 edges; Very large = 4000 vertices, 1000000 edges.
Timings are the fastest of the repeated runs, in seconds.

## Speed-up $S(P) = T_1 / T_P$

| Input size | $P=1$ | $P=2$ | $P=4$ | $P=8$ |
|------------|:-----:|:-----:|:-----:|:-----:|
| Small | 1.00 | 1.41 | 1.79 | 1.99 |
| Medium | 1.00 | 1.75 | 2.80 | 3.93 |
| Large | 1.00 | 1.88 | 3.37 | 5.53 |
| Very large | 1.00 | 1.93 | 3.65 | 6.47 |

## Runtime (seconds)

| Input size | $P=1$ | $P=2$ | $P=4$ | $P=8$ |
|------------|:-----:|:-----:|:-----:|:-----:|
| Small | 0.0361 | 0.0257 | 0.0202 | 0.0182 |
| Medium | 0.1965 | 0.1121 | 0.0703 | 0.0500 |
| Large | 0.6779 | 0.3611 | 0.2013 | 0.1226 |
| Very large | 1.7782 | 0.9210 | 0.4870 | 0.2748 |

## Efficiency $E(P) = S(P)/P$

| Input size | $P=1$ | $P=2$ | $P=4$ | $P=8$ |
|------------|:-----:|:-----:|:-----:|:-----:|
| Small | 1.00 | 0.70 | 0.45 | 0.25 |
| Medium | 1.00 | 0.88 | 0.70 | 0.49 |
| Large | 1.00 | 0.94 | 0.84 | 0.69 |
| Very large | 1.00 | 0.97 | 0.91 | 0.81 |

## Communication vs. computation

Percentage of each run spent inside MPI calls (broadcasting the edge list, plus the final reduce of the counts).

| Input size | $P=1$ | $P=2$ | $P=4$ | $P=8$ |
|------------|:-----:|:-----:|:-----:|:-----:|
| Small | 0.0% | 5.5% | 13.4% | 21.6% |
| Medium | 0.0% | 2.7% | 10.5% | 12.7% |
| Large | 0.0% | 1.3% | 4.8% | 5.9% |
| Very large | 0.0% | 0.6% | 2.5% | 4.0% |

