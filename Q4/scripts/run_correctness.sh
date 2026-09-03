#!/bin/bash

set -u

cd "$(dirname "$0")/.."

BIN=bin
DATA=data
OUT=results/correctness.txt

LAUNCH="${LAUNCH:-mpirun -np}"

WORK=results/.work

mkdir -p results "$WORK"
: > "$OUT"

log() { echo "$@" | tee -a "$OUT"; }

if [ ! -x "$BIN/tri_mpi" ]; then
    echo "bin/tri_mpi missing - run 'make' first" >&2
    exit 1
fi

log "Q4 correctness check - $(date)"
log ""

fails=0


log "== known-answer graphs =="

for case in sample tri_single tri_none tri_k4 tri_k5 tri_star tri_two; do
    want=$(cat "$DATA/$case.expected")

    for P in 1 2 4 8; do
        got=$($LAUNCH $P "$BIN/tri_mpi" "$DATA/$case.txt" 2>/dev/null | tr -d '[:space:]')

        if [ -n "$got" ] && [ "$got" = "$want" ]; then
            log "  PASS  $case  P=$P  ($got)"
        else
            log "  FAIL  $case  P=$P  got '$got' want '$want'"
            fails=$((fails + 1))
        fi
    done
done


log ""
log "== random graphs, MPI vs sequential =="

shapes="500,2000  1000,20000  300,10000  2000,4001  50,1000  5000,6000  100,4950"

seed=4242
for shape in $shapes; do
    Vv=${shape%%,*}          # text before the comma
    Ee=${shape##*,}          # text after the comma

    src="$WORK/rand_${Vv}_${Ee}.txt"

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


log ""
if [ "$fails" -eq 0 ]; then
    log "RESULT: all checks passed"
else
    log "RESULT: $fails check(s) failed"
fi
log ""
log "written to $OUT"

exit $fails
