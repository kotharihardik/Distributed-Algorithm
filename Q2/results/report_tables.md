# Q2 - Column-Row Matrix Multiplication: Results

Input sizes: Small = 500x500 times 500x500; Medium = 1000x1000 times 1000x1000; Large = 2000x2000 times 2000x2000; Very large = 4000x4000 times 4000x4000.
Timings are the fastest of the repeated runs, in seconds.

## Speed-up $S(P) = T_1 / T_P$

| Input size | $P=1$ | $P=2$ | $P=4$ | $P=8$ |
|------------|:-----:|:-----:|:-----:|:-----:|
| Small | 1.00 | 1.86 | 3.04 | 4.83 |
| Medium | 1.00 | 1.97 | 3.65 | 6.43 |
| Large | 1.00 | 2.00 | 3.89 | 7.34 |
| Very large | 1.00 | 1.99 | 3.95 | 7.68 |

## Runtime (seconds)

| Input size | $P=1$ | $P=2$ | $P=4$ | $P=8$ |
|------------|:-----:|:-----:|:-----:|:-----:|
| Small | 0.0681 | 0.0365 | 0.0224 | 0.0141 |
| Medium | 0.5334 | 0.2705 | 0.1463 | 0.0830 |
| Large | 4.2646 | 2.1291 | 1.0965 | 0.5808 |
| Very large | 34.1072 | 17.1217 | 8.6454 | 4.4430 |

## Efficiency $E(P) = S(P)/P$

| Input size | $P=1$ | $P=2$ | $P=4$ | $P=8$ |
|------------|:-----:|:-----:|:-----:|:-----:|
| Small | 1.00 | 0.93 | 0.76 | 0.60 |
| Medium | 1.00 | 0.99 | 0.91 | 0.80 |
| Large | 1.00 | 1.00 | 0.97 | 0.92 |
| Very large | 1.00 | 1.00 | 0.99 | 0.96 |

## Communication vs. computation

Percentage of each run spent inside MPI calls (scattering the column-row pairs, plus the final reduce).

| Input size | $P=1$ | $P=2$ | $P=4$ | $P=8$ |
|------------|:-----:|:-----:|:-----:|:-----:|
| Small | 1.5% | 6.3% | 22.0% | 34.6% |
| Medium | 0.6% | 2.2% | 8.4% | 16.9% |
| Large | 0.3% | 0.9% | 3.9% | 8.2% |
| Very large | 0.2% | 0.4% | 1.8% | 4.0% |

