#!/bin/bash
#
# Q4 - correctness check.  Does tri_mpi report the right number of triangles?
#
# Speed is not measured here at all; that is run_benchmark.sh's job.
#
# Everything is checked at P = 1, 2, 4 and 8, because a triangle counted twice
# by two different processes would only show up once the work is split.
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

# Scratch space for generated graphs and outputs.
#
# It has to sit under the project, NOT in /tmp. On a cluster /tmp is private to
# each machine, so a file written by a process on one node is invisible to this
# script running on another - and the comparison then comes back empty.
WORK=results/.work

mkdir -p results "$WORK"
: > "$OUT"

# Print to the screen and into the log file at the same time.
log() { echo "$@" | tee -a "$OUT"; }

if [ ! -x "$BIN/tri_mpi" ]; then
    echo "bin/tri_mpi missing - run 'make' first" >&2
    exit 1
fi

log "Q4 correctness check - $(date)"
log ""

fails=0


# ---------------------------------------------------------------------------
# Part 1 - graphs whose triangle count is known without a computer.
#
# Each data/<name>.txt has a data/<name>.expected holding the right answer.
# The complete graphs are the sharpest test: K5 must give exactly C(5,3) = 10,
# so any double counting or missed triangle changes the number immediately.
# ---------------------------------------------------------------------------
log "== known-answer graphs =="

for case in sample tri_single tri_none tri_k4 tri_k5 tri_star tri_two; do
    want=$(cat "$DATA/$case.expected")

    for P in 1 2 4 8; do
        got=$($LAUNCH $P "$BIN/tri_mpi" "$DATA/$case.txt" 2>/dev/null | tr -d '[:space:]')

        # -n "$got" matters: if the program crashed it prints nothing, and an
        # empty answer must count as a failure rather than quietly matching.
        if [ -n "$got" ] && [ "$got" = "$want" ]; then
            log "  PASS  $case  P=$P  ($got)"
        else
            log "  FAIL  $case  P=$P  got '$got' want '$want'"
            fails=$((fails + 1))
        fi
    done
done


# ---------------------------------------------------------------------------
# Part 2 - random graphs, checked against the sequential program.
#
# tri_seq uses a completely different method (plain adjacency lists, count the
# neighbours each edge's endpoints share, divide by three). Because it shares
# no logic with tri_mpi, the two agreeing is real evidence rather than a bug
# agreeing with itself.
#
# The shapes cover: small, dense, E not divisible by P, many isolated
# vertices, and a complete graph.
# ---------------------------------------------------------------------------
log ""
log "== random graphs, MPI vs sequential =="

shapes="500,2000  1000,20000  300,10000  2000,4001  50,1000  5000,6000  100,4950"

seed=4242
for shape in $shapes; do
    Vv=${shape%%,*}          # text before the comma
    Ee=${shape##*,}          # text after the comma

    src="$WORK/rand_${Vv}_${Ee}.txt"

    # A test that never ran must not report success. If the graph cannot be
    # built, or the reference gives no answer, both sides come back empty and
    # a plain string compare would happily call that a match.
    if ! "$BIN/gen_graph" "$Vv" "$Ee" "$seed" "$src" 2>/dev/null || [ ! -s "$src" ]; then
        log "  FAIL  V=$Vv E=$Ee  - could not generate the test graph"
        fails=$((fails + 1))
        seed=$((seed + 1))
        continue
    fi

    want=$("$BIN/tri_seq" "$src" 2>/dev/null | tr -d '[:space:]')
    if [ -z "$want" ]; then
        log "  FAIL  V=$Vv E=$Ee  - sequential reference produced no answer"
        fails=$((fails + 1))
        rm -f "$src"
        seed=$((seed + 1))
        continue
    fi

    # The count must be identical at every process count. If it drifts as P
    # changes, work is being duplicated or dropped somewhere.
    for P in 1 2 4 8; do
        got=$($LAUNCH $P "$BIN/tri_mpi" "$src" 2>/dev/null | tr -d '[:space:]')

        if [ -n "$got" ] && [ "$got" = "$want" ]; then
            log "  PASS  V=$Vv E=$Ee  P=$P  ($got triangles)"
        else
            log "  FAIL  V=$Vv E=$Ee  P=$P  got '$got' want '$want'"
            fails=$((fails + 1))
        fi
    done

    rm -f "$src"
    seed=$((seed + 1))       # a different graph for the next shape
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
