#!/bin/bash

set -u

cd "$(dirname "$0")/.."

BIN=bin
DATA=data
RES=results

LAUNCH="${LAUNCH:-mpirun -np}"   # Makefile supplies the machine's flags
REPS="${REPS:-3}"                # runs per measurement, fastest wins
SEED="${SEED:-12345}"            # fixes which logs get generated

mkdir -p "$RES" "$DATA/generated"

CSV="$RES/benchmark_raw.csv"
LOG="$RES/benchmark_log.txt"

if [ ! -x "$BIN/log_mpi" ]; then
    echo "bin/log_mpi missing - run 'make' first" >&2
    exit 1
fi


CASES="
Small:10000:64
Medium:100000:64
Large:1000000:64
Verylarge:10000000:64
"

TOPK=10          # K, the size of each top list
ENDPOINTS=500    # distinct endpoint ids


echo "label,N,S,procs,rep,read,compute,reduce,total" > "$CSV"
{
    echo "Q7 server log analytics benchmark"
    echo "started : $(date)"
    echo "host    : $(hostname)"
    echo "reps    : $REPS (fastest kept)"
    echo "seed    : $SEED"
    echo
} > "$LOG"


for case in $CASES; do
    label=$(echo "$case" | cut -d: -f1)
    Nn=$(echo "$case" | cut -d: -f2)
    Ss=$(echo "$case" | cut -d: -f3)

    src="$DATA/generated/${label}.txt"
    if [ ! -f "$src" ]; then
        echo "generating $label (N=$Nn S=$Ss) ..." | tee -a "$LOG"
        "$BIN/gen_log" "$Nn" "$TOPK" "$Ss" "$SEED" "$src" "$ENDPOINTS" 2>>"$LOG"
    fi

    echo "" | tee -a "$LOG"
    echo "--- $label  N=$Nn  S=$Ss ---" | tee -a "$LOG"

    for P in 1 2 4 8; do
        best=""

        for rep in $(seq 1 "$REPS"); do
            timing=$($LAUNCH $P "$BIN/log_mpi" "$src" --time 2>&1 >/dev/null)

            rd=$(echo "$timing" | awk '/^read/    {print $2}')  # each rank reads its slice
            cp=$(echo "$timing" | awk '/^compute/ {print $2}')  # parse and tally
            re=$(echo "$timing" | awk '/^reduce/  {print $2}')  # merge the tallies
            tt=$(echo "$timing" | awk '/^total/   {print $2}')

            if [ -z "$tt" ]; then
                echo "  P=$P rep=$rep FAILED" | tee -a "$LOG"
                continue
            fi

            rd=${rd:-0}; cp=${cp:-0}; re=${re:-0}

            echo "$label,$Nn,$Ss,$P,$rep,$rd,$cp,$re,$tt" >> "$CSV"

            if [ -z "$best" ] || awk "BEGIN{exit !($tt < $best)}"; then
                best=$tt; b_rd=$rd; b_cp=$cp; b_re=$re
            fi
        done

        if [ -n "$best" ]; then
            printf "  P=%-2s total %8.4fs   read %7.4f  compute %7.4f  reduce %7.4f\n" \
                   "$P" "$best" "$b_rd" "$b_cp" "$b_re" | tee -a "$LOG"
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
