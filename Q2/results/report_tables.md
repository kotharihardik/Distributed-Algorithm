# Q2 - Column-Row Matrix Multiplication: Results

Input sizes: 
- Tiny = 100x100 times 100x100; 
- Small = 500x500 times 500x500; 
- Medium = 1000x1000 times 1000x1000; 
- Large = 2000x2000 times 2000x2000; 
- Very large = 4000x4000 times 4000x4000.

Timings are the fastest of the repeated runs, in seconds.

## Speed-up $S(P) = T_1 / T_P$

| Input size | $P=1$ | $P=2$ | $P=4$ | $P=8$ |
|------------|:-----:|:-----:|:-----:|:-----:|
| Tiny | 1.00 | 1.35 | 1.29 | 0.99 |
| Small | 1.00 | 1.89 | 3.13 | 5.03 |
| Medium | 1.00 | 1.89 | 3.52 | 4.54 |
| Large | 1.00 | 1.78 | 3.46 | 5.55 |
| Very large | 1.00 | 1.80 | 3.58 | 5.89 |

## Runtime (seconds)

| Input size | $P=1$ | $P=2$ | $P=4$ | $P=8$ |
|------------|:-----:|:-----:|:-----:|:-----:|
| Tiny | 0.0006 | 0.0004 | 0.0005 | 0.0006 |
| Small | 0.0695 | 0.0368 | 0.0222 | 0.0138 |
| Medium | 0.5412 | 0.2861 | 0.1536 | 0.1193 |
| Large | 5.6950 | 3.1932 | 1.6480 | 1.0262 |
| Very large | 47.9148 | 26.5931 | 13.3929 | 8.1413 |

## Efficiency $E(P) = S(P)/P$

| Input size | $P=1$ | $P=2$ | $P=4$ | $P=8$ |
|------------|:-----:|:-----:|:-----:|:-----:|
| Tiny | 1.00 | 0.68 | 0.32 | 0.12 |
| Small | 1.00 | 0.94 | 0.78 | 0.63 |
| Medium | 1.00 | 0.95 | 0.88 | 0.57 |
| Large | 1.00 | 0.89 | 0.86 | 0.69 |
| Very large | 1.00 | 0.90 | 0.89 | 0.74 |

## Communication vs. computation

Percentage of each run spent inside MPI calls (scattering the column-row pairs, plus the final reduce).

| Input size | $P=1$ | $P=2$ | $P=4$ | $P=8$ |
|------------|:-----:|:-----:|:-----:|:-----:|
| Tiny | 5.6% | 33.0% | 65.2% | 90.0% |
| Small | 1.3% | 5.8% | 20.5% | 33.3% |
| Medium | 0.5% | 2.0% | 7.7% | 15.2% |
| Large | 0.2% | 0.6% | 3.1% | 4.7% |
| Very large | 0.1% | 0.4% | 1.3% | 2.6% |

