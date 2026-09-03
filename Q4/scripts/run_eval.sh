#!/bin/bash

set -u

cd "$(dirname "$0")/.."

BIN=bin
RES=results

LAUNCH="${LAUNCH:-mpirun -np}"          # Makefile supplies the machine's flags
REPS="${REPS:-3}"                       # runs per process count, fastest wins
PROCS_LIST="${PROCS_LIST:-1 2 4 8}"
GRAPH="${1:-data/graph.txt}"

OUT="$RES/eval_run.txt"
OUTCSV="$RES/eval_raw.csv"


ERR="$RES/.eval_err"
BEST="$RES/.eval_best"

mkdir -p "$RES"

if [ ! -x "$BIN/tri_mpi" ]; then
    echo "bin/tri_mpi missing - run 'make' first" >&2
    exit 1
fi

if [ ! -f "$GRAPH" ]; then
    echo "no such graph file: $GRAPH" >&2
    echo "usage: bash scripts/run_eval.sh <graph.txt>" >&2
    exit 1
fi

: > "$OUT"
: > "$BEST"
echo "procs,rep,triangles,read,bcast,build,count,reduce,total" > "$OUTCSV"

say() { echo "$@" | tee -a "$OUT"; }

say "Q4 - triangle counting on a supplied graph"
say "file  : $GRAPH"
say "date  : $(date)"
say "host  : $(hostname)"
say "reps  : $REPS (fastest of each kept)"
say "procs : $PROCS_LIST"
say ""
say "first line of the file (V and E as declared): $(head -1 "$GRAPH")"
say ""

answer=""
disagree=0
failed=0

for P in $PROCS_LIST; do
    best=""

    for rep in $(seq 1 "$REPS"); do
        count=$($LAUNCH $P "$BIN/tri_mpi" "$GRAPH" --time 2>"$ERR" | tr -d '[:space:]')
        timing=$(cat "$ERR")

        rd=$(echo "$timing" | awk '/^read/   {print $2}')
        bc=$(echo "$timing" | awk '/^bcast/  {print $2}')
        bd=$(echo "$timing" | awk '/^build/  {print $2}')
        ct=$(echo "$timing" | awk '/^count/  {print $2}')
        re=$(echo "$timing" | awk '/^reduce/ {print $2}')
        tt=$(echo "$timing" | awk '/^total/  {print $2}')


        if [ -z "$tt" ] || [ -z "$count" ]; then
            say "  P=$P rep=$rep FAILED"
            failed=$((failed + 1))
            continue
        fi

        rd=${rd:-0}; bc=${bc:-0}; bd=${bd:-0}; ct=${ct:-0}; re=${re:-0}

        echo "$P,$rep,$count,$rd,$bc,$bd,$ct,$re,$tt" >> "$OUTCSV"

        if [ -z "$answer" ]; then
            answer=$count
        elif [ "$count" != "$answer" ]; then
            disagree=1
            say "  P=$P rep=$rep MISMATCH: got $count, earlier runs said $answer"
        fi

        if [ -z "$best" ] || awk "BEGIN{exit !($tt < $best)}"; then
            best=$tt; b_rd=$rd; b_bc=$bc; b_bd=$bd; b_ct=$ct; b_re=$re
        fi
    done

    if [ -n "$best" ]; then
        say "  P=$P  best total ${best}s"
        echo "$P $best $b_rd $b_bc $b_bd $b_ct $b_re" >> "$BEST"
    fi
done

rm -f "$ERR"

say ""
say "==================================================================="
say " TRIANGLES : ${answer:-<no answer - every run failed>}"
say "==================================================================="

if [ "$disagree" -ne 0 ]; then
    say " WARNING: the count changed between runs - the timings below cannot"
    say "          be trusted."
fi

edges=$(head -1 "$GRAPH" | awk '{print $2+0}')
if [ -x "$BIN/tri_seq" ] && [ "$edges" -le 1000000 ]; then
    sq=$("$BIN/tri_seq" "$GRAPH" 2>/dev/null | tr -d '[:space:]')
    if [ "$sq" = "$answer" ]; then
        say " sequential check (bin/tri_seq): agrees ($sq)"
    else
        say " sequential check (bin/tri_seq): DISAGREES - it says '$sq'"
        disagree=1
    fi
fi


if [ -s "$BEST" ]; then
    say ""
    awk '
    { n++; p[n]=$1; t[n]=$2; rd[n]=$3; bc[n]=$4; bd[n]=$5; ct[n]=$6; re[n]=$7
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
        printf "  %-6s %9s %9s %9s %9s %9s %8s\n", \
               "P", "read", "bcast", "build", "count", "reduce", "comm %"
        printf "  %-6s %9s %9s %9s %9s %9s %8s\n", \
               "-----", "-------", "-------", "-------", "-------", "-------", "------"
        for (i = 1; i <= n; i++) {
            comm = (t[i] > 0) ? 100*(bc[i]+re[i])/t[i] : 0
            printf "  %-6d %9.4f %9.4f %9.4f %9.4f %9.4f %7.1f%%\n", \
                   p[i], rd[i], bc[i], bd[i], ct[i], re[i], comm
        }
    }' "$BEST" | tee -a "$OUT"
fi

rm -f "$BEST"

say ""
say "transcript  : $OUT"
say "raw timings : $OUTCSV"

if [ "$disagree" -ne 0 ] || [ -z "$answer" ] || [ "$failed" -ne 0 ]; then
    exit 1
fi
exit 0
