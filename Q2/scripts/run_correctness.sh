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

if [ ! -x "$BIN/matmul_mpi" ]; then
    echo "bin/matmul_mpi missing - run 'make' first" >&2
    exit 1
fi

same_numbers() {
    diff -q <(tr -s ' \n' ' ' < "$1") <(tr -s ' \n' ' ' < "$2") >/dev/null 2>&1
}

log "Q2 correctness check - $(date)"
log ""

fails=0


log "== supplied examples and edge cases =="

for case in example1 example2 edge_n1 edge_m1 edge_p1; do
    for P in 1 2 4 8; do
        got=$WORK/${case}_p${P}.out
        $LAUNCH $P "$BIN/matmul_mpi" "$DATA/$case.txt" -o "$got" 2>/dev/null

        if [ -s "$got" ] && same_numbers "$got" "$DATA/$case.expected"; then
            log "  PASS  $case  P=$P"
        else
            log "  FAIL  $case  P=$P"
            fails=$((fails + 1))
        fi
        rm -f "$got"
    done
done


log ""
log "== random cases, MPI vs sequential =="

shapes="60,60,60  100,64,80  100,63,80  400,20,30  20,400,30  1,50,40  30,1,40  50,37,1"

seed=2024
for shape in $shapes; do
    m=${shape%%,*}          
    rest=${shape#*,}
    n=${rest%%,*}            
    p=${rest##*,}            

    src=$WORK/rand_${m}_${n}_${p}.txt


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
