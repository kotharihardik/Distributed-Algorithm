#!/bin/bash


set -u

cd "$(dirname "$0")/.."

BIN=bin
DATA=data
RES=results

LAUNCH="${LAUNCH:-mpirun -np}"  
REPS="${REPS:-3}"                
SEED="${SEED:-12345}"           

mkdir -p "$RES" "$DATA/generated"

CSV="$RES/benchmark_raw.csv"
LOG="$RES/benchmark_log.txt"

if [ ! -x "$BIN/matmul_mpi" ]; then
    echo "bin/matmul_mpi missing - run 'make' first" >&2
    exit 1
fi


CASES="
Tiny:100:100:100
Small:500:500:500
Medium:1000:1000:1000
Large:2000:2000:2000
Verylarge:4000:4000:4000
"


echo "label,m,n,p,procs,rep,scatter,compute,reduce,total" > "$CSV"
{
    echo "Q2 column-row matrix multiplication benchmark"
    echo "started : $(date)"
    echo "host    : $(hostname)"
    echo "reps    : $REPS (fastest kept)"
    echo "seed    : $SEED"
    echo
} > "$LOG"


for case in $CASES; do
    label=$(echo "$case" | cut -d: -f1)
    m=$(echo "$case" | cut -d: -f2)
    n=$(echo "$case" | cut -d: -f3)
    p=$(echo "$case" | cut -d: -f4)

    src="$DATA/generated/${label}.txt"
    if [ ! -f "$src" ]; then
        echo "generating $label (${m}x${n}x${p}) ..." | tee -a "$LOG"
        "$BIN/gen_matrix" "$m" "$n" "$p" "$SEED" "$src" 2>>"$LOG"
    fi

    echo "" | tee -a "$LOG"
    echo "--- $label  A=${m}x${n}  B=${n}x${p} ---" | tee -a "$LOG"

    for P in 1 2 4 8; do
        best=""

        for rep in $(seq 1 "$REPS"); do
            
            timing=$($LAUNCH $P "$BIN/matmul_mpi" "$src" -o /dev/null --time 2>&1 >/dev/null)

            sc=$(echo "$timing" | awk '/^scatter/ {print $2}') 
            cp=$(echo "$timing" | awk '/^compute/ {print $2}')  
            re=$(echo "$timing" | awk '/^reduce/  {print $2}') 
            tt=$(echo "$timing" | awk '/^total/   {print $2}')

            if [ -z "$tt" ]; then
                echo "  P=$P rep=$rep FAILED" | tee -a "$LOG"
                continue
            fi


            sc=${sc:-0}; cp=${cp:-0}; re=${re:-0}

            echo "$label,$m,$n,$p,$P,$rep,$sc,$cp,$re,$tt" >> "$CSV"

            if [ -z "$best" ] || awk "BEGIN{exit !($tt < $best)}"; then
                best=$tt; b_sc=$sc; b_cp=$cp; b_re=$re
            fi
        done

        if [ -n "$best" ]; then
            printf "  P=%-2s total %8.4fs   scatter %7.4f  compute %7.4f  reduce %7.4f\n" \
                   "$P" "$best" "$b_sc" "$b_cp" "$b_re" | tee -a "$LOG"
        fi
    done
done

echo "" | tee -a "$LOG"
echo "finished: $(date)" | tee -a "$LOG"
echo "" | tee -a "$LOG"
echo "raw timings : $CSV" | tee -a "$LOG"
echo "transcript  : $LOG" | tee -a "$LOG"


if command -v python3 >/dev/null 2>&1 && [ -f scripts/summarize.py ]; then
    python3 scripts/summarize.py "$CSV" > "$RES/report_tables.md"
    echo "tables      : $RES/report_tables.md" | tee -a "$LOG"
else
    echo "tables      : skipped (scripts/summarize.py not present)" | tee -a "$LOG"
fi

if [ -f scripts/plot_speedup.py ] && python3 -c "import matplotlib" 2>/dev/null; then
    python3 scripts/plot_speedup.py "$CSV" "$RES/speedup.png" >/dev/null 2>&1 \
        && echo "graph       : $RES/speedup.png" | tee -a "$LOG"
else
    echo "graph       : skipped (needs scripts/plot_speedup.py and matplotlib)" | tee -a "$LOG"
fi
