#!/bin/bash
#
# Q7 - correctness check.  Does log_mpi produce the same report as log_seq?
#
# Speed is not measured here at all; that is run_benchmark.sh's job.
#
# Everything runs at P = 1, 2, 4 and 8. The process count matters more here
# than it looks: the log is split by byte offset, so a boundary landing in the
# middle of a line is exactly the kind of bug that only appears once P > 1.
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

LAUNCH="${LAUNCH:-mpirun -np}"

# Scratch space on the shared filesystem, never /tmp: on a cluster /tmp is
# private to each machine, so a file written by a process on one node is
# invisible to this script running on another.
WORK=results/.work

mkdir -p results "$WORK"
: > "$OUT"

log() { echo "$@" | tee -a "$OUT"; }

if [ ! -x "$BIN/log_mpi" ]; then
    echo "bin/log_mpi missing - run 'make' first" >&2
    exit 1
fi

log "Q7 correctness check - $(date)"
log ""

fails=0


# ---------------------------------------------------------------------------
# Part 1 - logs small enough to work out by hand.
#
# Each data/<name>.txt has a data/<name>.expected written out by hand. Between
# them they cover: the worked example, every request succeeding, every request
# failing, a single line, and a case where counts tie so the "smaller id wins"
# rule has to do some work.
# ---------------------------------------------------------------------------
log "== hand-checked logs =="

for case in sample all_ok all_fail single ties; do
    for P in 1 2 4 8; do
        got="$WORK/${case}_p${P}.out"
        $LAUNCH $P "$BIN/log_mpi" "$DATA/$case.txt" > "$got" 2>/dev/null

        # -s catches a crash: an empty file must fail, not silently match
        if [ -s "$got" ] && diff -q "$got" "$DATA/$case.expected" >/dev/null 2>&1; then
            log "  PASS  $case  P=$P"
        else
            log "  FAIL  $case  P=$P"
            fails=$((fails + 1))
        fi
        rm -f "$got"
    done
done


# ---------------------------------------------------------------------------
# Part 2 - generated logs, checked line for line against the sequential run.
#
# log_seq reads the whole file in one process with no byte-range splitting at
# all, so if the two reports differ the fault is in how the work was divided.
#
# The shapes cover: tiny, N not divisible by P, more servers than lines, more
# endpoints than lines, and a log big enough to span many processes.
# ---------------------------------------------------------------------------
log ""
log "== generated logs, MPI vs sequential =="

# N,K,S,endpoints
shapes="3,2,2,2  137,3,7,11  1000,5,8,200  9973,10,16,64  50000,10,32,500  20000,5,300,4000"

seed=7001
for shape in $shapes; do
    N=$(echo "$shape" | cut -d, -f1)
    K=$(echo "$shape" | cut -d, -f2)
    S=$(echo "$shape" | cut -d, -f3)
    E=$(echo "$shape" | cut -d, -f4)

    src="$WORK/log_${N}_${S}.txt"

    # A test that never ran must not report success. If the log cannot be
    # generated, or the reference produces nothing, both sides come back empty
    # and a plain comparison would wrongly call that a match.
    if ! "$BIN/gen_log" "$N" "$K" "$S" "$seed" "$src" "$E" 2>/dev/null || [ ! -s "$src" ]; then
        log "  FAIL  N=$N S=$S  - could not generate the test log"
        fails=$((fails + 1))
        seed=$((seed + 1))
        continue
    fi

    ref="$WORK/ref.out"
    "$BIN/log_seq" "$src" > "$ref" 2>/dev/null
    if [ ! -s "$ref" ]; then
        log "  FAIL  N=$N S=$S  - sequential reference produced no report"
        fails=$((fails + 1))
        rm -f "$src"
        seed=$((seed + 1))
        continue
    fi

    for P in 1 2 4 8; do
        got="$WORK/mpi_p${P}.out"
        $LAUNCH $P "$BIN/log_mpi" "$src" > "$got" 2>/dev/null

        if [ -s "$got" ] && diff -q "$got" "$ref" >/dev/null 2>&1; then
            log "  PASS  N=$N K=$K S=$S E=$E  P=$P"
        else
            log "  FAIL  N=$N K=$K S=$S E=$E  P=$P"
            diff "$got" "$ref" 2>/dev/null | head -4 | sed 's/^/        /' | tee -a "$OUT"
            fails=$((fails + 1))
        fi
        rm -f "$got"
    done

    rm -f "$src" "$ref"
    seed=$((seed + 1))
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
