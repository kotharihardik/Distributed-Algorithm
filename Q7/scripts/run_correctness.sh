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

if [ ! -x "$BIN/log_mpi" ]; then
    echo "bin/log_mpi missing - run 'make' first" >&2
    exit 1
fi

log "Q7 correctness check - $(date)"
log ""

fails=0


log "== hand-checked logs =="

for case in sample all_ok all_fail single ties; do
    for P in 1 2 4 8; do
        got="$WORK/${case}_p${P}.out"
        $LAUNCH $P "$BIN/log_mpi" "$DATA/$case.txt" > "$got" 2>/dev/null

        if [ -s "$got" ] && diff -q "$got" "$DATA/$case.expected" >/dev/null 2>&1; then
            log "  PASS  $case  P=$P"
        else
            log "  FAIL  $case  P=$P"
            fails=$((fails + 1))
        fi
        rm -f "$got"
    done
done


log ""
log "== generated logs, MPI vs sequential =="

shapes="3,2,2,2  137,3,7,11  1000,5,8,200  9973,10,16,64  50000,10,32,500  20000,5,300,4000"

seed=7001
for shape in $shapes; do
    N=$(echo "$shape" | cut -d, -f1)
    K=$(echo "$shape" | cut -d, -f2)
    S=$(echo "$shape" | cut -d, -f3)
    E=$(echo "$shape" | cut -d, -f4)

    src="$WORK/log_${N}_${S}.txt"

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


log ""
if [ "$fails" -eq 0 ]; then
    log "RESULT: all checks passed"
else
    log "RESULT: $fails check(s) failed"
fi
log ""
log "written to $OUT"

exit $fails
