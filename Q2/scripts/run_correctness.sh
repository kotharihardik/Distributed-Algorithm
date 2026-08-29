#!/bin/bash
#
# Q2 - correctness check.  Does matmul_mpi produce the right matrix C?
#
# Speed is not measured here at all; that is run_benchmark.sh's job.
#
# Everything is checked at P = 1, 2, 4 and 8, because a wrong split of the
# column-row pairs only shows up once the work is actually divided.
#
#   make test
#   bash scripts/run_correctness.sh
#   LAUNCH="mpirun -np" bash scripts/run_correctness.sh     # override launcher
#
# Exits 0 when everything passed, otherwise the number of failures.

set -u

cd "$(dirname "$0")/.."

BIN=bin
DATA=data
OUT=results/correctness.txt

# How to start N MPI processes. The Makefile passes the right flags for the
# machine; this default is only used when the script is run by hand.
LAUNCH="${LAUNCH:-mpirun -np}"

# Scratch space for generated inputs and outputs.
#
# It has to sit under the project, NOT in /tmp. On a cluster /tmp is private to
# each machine, so a file written by a process on one node is invisible to this
# script running on another - and the comparison then comes back empty.
WORK=results/.work

mkdir -p results "$WORK"
: > "$OUT"

# Print to the screen and into the log file at the same time.
log() { echo "$@" | tee -a "$OUT"; }

if [ ! -x "$BIN/matmul_mpi" ]; then
    echo "bin/matmul_mpi missing - run 'make' first" >&2
    exit 1
fi

# Two result files match if their numbers match. Whitespace is squeezed first
# so a difference in spacing or line breaks is not mistaken for a wrong answer.
same_numbers() {
    diff -q <(tr -s ' \n' ' ' < "$1") <(tr -s ' \n' ' ' < "$2") >/dev/null 2>&1
}

log "Q2 correctness check - $(date)"
log ""

fails=0


# ---------------------------------------------------------------------------
# Part 1 - inputs whose answer is written down by hand.
#
# Each data/<name>.txt has a data/<name>.expected next to it. These are the two
# worked examples from the question paper plus the edge cases the constraints
# call out: a single column-row pair (n=1), a single row (m=1), a single
# column (p=1).
# ---------------------------------------------------------------------------
log "== supplied examples and edge cases =="

for case in example1 example2 edge_n1 edge_m1 edge_p1; do
    for P in 1 2 4 8; do
        got=$WORK/${case}_p${P}.out
        $LAUNCH $P "$BIN/matmul_mpi" "$DATA/$case.txt" -o "$got" 2>/dev/null

        # -s checks the file exists and is non-empty: if the program crashed it
        # writes nothing, and two empty files would otherwise compare equal.
        if [ -s "$got" ] && same_numbers "$got" "$DATA/$case.expected"; then
            log "  PASS  $case  P=$P"
        else
            log "  FAIL  $case  P=$P"
            fails=$((fails + 1))
        fi
        rm -f "$got"
    done
done


# ---------------------------------------------------------------------------
# Part 2 - random matrices, checked against the sequential program.
#
# matmul_seq runs the same maths on one core with no communication, so if the
# two disagree the fault is in how the work was split or collected.
#
# The shapes cover: square, n divisible by P, n NOT divisible by P, tall
# (m >> n), wide (n >> m), and single-row / single-column inputs.
# ---------------------------------------------------------------------------
log ""
log "== random cases, MPI vs sequential =="

shapes="60,60,60  100,64,80  100,63,80  400,20,30  20,400,30  1,50,40  30,1,40  50,37,1"

seed=2024
for shape in $shapes; do
    m=${shape%%,*}           # first number
    rest=${shape#*,}
    n=${rest%%,*}            # middle number
    p=${rest##*,}            # last number

    src=$WORK/rand_${m}_${n}_${p}.txt

    # A test that never ran must not report success. If the input cannot be
    # built, or the reference produces nothing, both sides of the comparison
    # come back empty and would wrongly count as a match.
    if ! "$BIN/gen_matrix" "$m" "$n" "$p" "$seed" "$src" 2>/dev/null || [ ! -s "$src" ]; then
        log "  FAIL  ${m}x${n}x${p}  - could not generate the test input"
        fails=$((fails + 1))
        seed=$((seed + 1))
        continue
    fi

    "$BIN/matmul_seq" "$src" -o $WORK/ref.out 2>/dev/null
    if [ ! -s $WORK/ref.out ]; then
        log "  FAIL  ${m}x${n}x${p}  - sequential reference produced no output"
        fails=$((fails + 1))
        rm -f "$src"
        seed=$((seed + 1))
        continue
    fi

    # The matrix must come out identical at every process count.
    for P in 1 2 4 8; do
        got=$WORK/mpi_p${P}.out
        $LAUNCH $P "$BIN/matmul_mpi" "$src" -o "$got" 2>/dev/null

        if [ -s "$got" ] && same_numbers "$got" "$WORK/ref.out"; then
            log "  PASS  ${m}x${n}x${p}  P=$P"
        else
            log "  FAIL  ${m}x${n}x${p}  P=$P"
            fails=$((fails + 1))
        fi
        rm -f "$got"
    done

    rm -f "$src" $WORK/ref.out
    seed=$((seed + 1))       # different numbers for the next shape
done


# ---------------------------------------------------------------------------
# Verdict
# ---------------------------------------------------------------------------
log ""
if [ "$fails" -eq 0 ]; then
    log "RESULT: all checks passed"
else
    log "RESULT: $fails check(s) failed"
fi
log ""
log "written to $OUT"

exit $fails
