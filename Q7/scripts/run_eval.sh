#!/bin/bash

set -u

cd "$(dirname "$0")/.."

BIN=bin
RES=results

LAUNCH="${LAUNCH:-mpirun -np}"          # Makefile supplies the machine's flags
REPS="${REPS:-3}"                       # runs per process count, fastest wins
PROCS_LIST="${PROCS_LIST:-1 2 4 8}"
LOG="${1:-data/server_log.txt}"

OUT="$RES/eval_run.txt"
OUTCSV="$RES/eval_raw.csv"

WORK="$RES/.work/eval"    # under results/.work, which .gitignore excludes
ERR="$WORK/err"
BEST="$WORK/best"
REPORT="$WORK/report"

mkdir -p "$RES" "$WORK"

if [ ! -x "$BIN/log_mpi" ]; then
    echo "bin/log_mpi missing - run 'make' first" >&2
    exit 1
fi

if [ ! -f "$LOG" ]; then
    echo "no such log file: $LOG" >&2
    echo "usage: bash scripts/run_eval.sh <server_log.txt>" >&2
    exit 1
fi

: > "$OUT"
: > "$BEST"
rm -f "$REPORT"
echo "procs,rep,total_requests,read,compute,reduce,total" > "$OUTCSV"

say() { echo "$@" | tee -a "$OUT"; }

say "Q7 - server log analytics on a supplied log"
say "file  : $LOG"
say "date  : $(date)"
say "host  : $(hostname)"
say "reps  : $REPS (fastest of each kept)"
say "procs : $PROCS_LIST"
say ""
say "first line of the file (N, K and S as declared): $(head -1 "$LOG")"
say ""

disagree=0
failed=0

for P in $PROCS_LIST; do
    best=""

    for rep in $(seq 1 "$REPS"); do
        got="$WORK/p${P}_r${rep}.out"
        $LAUNCH $P "$BIN/log_mpi" "$LOG" --time > "$got" 2>"$ERR"
        timing=$(cat "$ERR")

        rd=$(echo "$timing" | awk '/^read/    {print $2}')
        cp=$(echo "$timing" | awk '/^compute/ {print $2}')
        re=$(echo "$timing" | awk '/^reduce/  {print $2}')
        tt=$(echo "$timing" | awk '/^total/   {print $2}')
        req=$(awk '/^TOTAL_REQUESTS/ {print $2}' "$got")

        if [ -z "$tt" ] || [ ! -s "$got" ] || [ -z "$req" ]; then
            say "  P=$P rep=$rep FAILED"
            failed=$((failed + 1))
            rm -f "$got"
            continue
        fi

        rd=${rd:-0}; cp=${cp:-0}; re=${re:-0}

        echo "$P,$rep,$req,$rd,$cp,$re,$tt" >> "$OUTCSV"

        # split is by byte offset, so a mid-line boundary only shows up at P>1
        if [ ! -f "$REPORT" ]; then
            cp "$got" "$REPORT"
        elif ! diff -q "$got" "$REPORT" >/dev/null 2>&1; then
            disagree=1
            say "  P=$P rep=$rep MISMATCH: report differs from the P=1 run"
            diff "$REPORT" "$got" | head -6 | sed 's/^/        /' | tee -a "$OUT"
        fi
        rm -f "$got"

        if [ -z "$best" ] || awk "BEGIN{exit !($tt < $best)}"; then
            best=$tt; b_rd=$rd; b_cp=$cp; b_re=$re
        fi
    done

    if [ -n "$best" ]; then
        say "  P=$P  best total ${best}s"
        echo "$P $best $b_rd $b_cp $b_re" >> "$BEST"
    fi
done

say ""
say "==================================================================="
if [ -f "$REPORT" ]; then
    cat "$REPORT" | tee -a "$OUT"
else
    say " no report - every run failed"
fi
say "==================================================================="

if [ "$disagree" -ne 0 ]; then
    say " WARNING: the report changed with the process count - the timings"
    say "          below cannot be trusted."
fi

if [ -x "$BIN/log_seq" ] && [ -f "$REPORT" ]; then
    "$BIN/log_seq" "$LOG" > "$WORK/seq.out" 2>/dev/null
    if diff -q "$WORK/seq.out" "$REPORT" >/dev/null 2>&1; then
        say " sequential check (bin/log_seq): identical report"
    else
        say " sequential check (bin/log_seq): DIFFERS"
        diff "$WORK/seq.out" "$REPORT" | head -6 | sed 's/^/        /' | tee -a "$OUT"
        disagree=1
    fi
fi

if [ -s "$BEST" ]; then
    say ""
    awk '
    { n++; p[n]=$1; t[n]=$2; rd[n]=$3; cp[n]=$4; re[n]=$5
      if (p[n] == 1) base = t[n] }
    END {
        if (base == 0) base = t[1]      # no P=1 run: fall back to the first

        print "Runtime, speed-up and efficiency"
        print ""
        printf "  %-6s %12s %10s %12s\n", "P", "time (s)", "speed-up", "efficiency"
        printf "  %-6s %12s %10s %12s\n", "-----", "----------", "--------", "----------"
        for (i = 1; i <= n; i++)
            printf "  %-6d %12.4f %10.2f %11.2f\n", \
                   p[i], t[i], base/t[i], (base/t[i])/p[i]

        print ""
        print "Where the time went (seconds, and MPI as a share of the run)"
        print ""
        printf "  %-6s %11s %11s %11s %9s\n", \
               "P", "read", "compute", "reduce", "comm %"
        printf "  %-6s %11s %11s %11s %9s\n", \
               "-----", "---------", "---------", "---------", "-------"
        for (i = 1; i <= n; i++) {
            comm = (t[i] > 0) ? 100*re[i]/t[i] : 0
            printf "  %-6d %11.4f %11.4f %11.4f %8.1f%%\n", \
                   p[i], rd[i], cp[i], re[i], comm
        }
    }' "$BEST" | tee -a "$OUT"
fi

rm -rf "$WORK"

say ""
say "transcript  : $OUT"
say "raw timings : $OUTCSV"

if [ "$disagree" -ne 0 ] || [ ! -s "$OUTCSV" ] || [ "$failed" -ne 0 ]; then
    exit 1
fi
exit 0
