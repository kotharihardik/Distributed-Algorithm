# Q2 - Column-Row Matrix Multiplication: Results

Input sizes: Small = 500x500 times 500x500; Medium = 1000x1000 times 1000x1000; Large = 2000x2000 times 2000x2000; Very large = 4000x4000 times 4000x4000.
Timings are the fastest of the repeated runs, in seconds.

## Speed-up $S(P) = T_1 / T_P$

| Input size | $P=1$ | $P=2$ | $P=4$ | $P=8$ |
|------------|:-----:|:-----:|:-----:|:-----:|
| Small | 1.00 | 1.88 | 3.09 | 4.87 |
| Medium | 1.00 | 1.90 | 3.47 | 4.52 |
| Large | 1.00 | 1.77 | 3.46 | 5.53 |
| Very large | 1.00 | 1.81 | 3.59 | 5.90 |

## Runtime (seconds)

| Input size | $P=1$ | $P=2$ | $P=4$ | $P=8$ |
|------------|:-----:|:-----:|:-----:|:-----:|
| Small | 0.0696 | 0.0370 | 0.0226 | 0.0143 |
| Medium | 0.5410 | 0.2850 | 0.1559 | 0.1198 |
| Large | 5.6956 | 3.2213 | 1.6449 | 1.0306 |
| Very large | 47.9416 | 26.4314 | 13.3534 | 8.1199 |

## Efficiency $E(P) = S(P)/P$

| Input size | $P=1$ | $P=2$ | $P=4$ | $P=8$ |
|------------|:-----:|:-----:|:-----:|:-----:|
| Small | 1.00 | 0.94 | 0.77 | 0.61 |
| Medium | 1.00 | 0.95 | 0.87 | 0.56 |
| Large | 1.00 | 0.88 | 0.87 | 0.69 |
| Very large | 1.00 | 0.91 | 0.90 | 0.74 |

## Communication vs. computation

Percentage of each run spent inside MPI calls (scattering the column-row pairs, plus the final reduce).

| Input size | $P=1$ | $P=2$ | $P=4$ | $P=8$ |
|------------|:-----:|:-----:|:-----:|:-----:|
| Small | 1.4% | 6.1% | 21.2% | 33.5% |
| Medium | 0.6% | 2.1% | 8.2% | 14.2% |
| Large | 0.2% | 0.6% | 2.6% | 5.3% |
| Very large | 0.1% | 0.4% | 1.4% | 2.4% |

