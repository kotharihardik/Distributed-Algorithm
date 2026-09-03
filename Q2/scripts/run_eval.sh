#!/bin/bash

set -u

cd "$(dirname "$0")/.."

BIN=bin
RES=results

LAUNCH="${LAUNCH:-mpirun -np}"
REPS="${REPS:-3}"
PROCS_LIST="${PROCS_LIST:-1 2 4 8}"
INPUT="${1:-data/input.txt}"

OUT="$RES/eval_run.txt"
OUTCSV="$RES/eval_raw.csv"
PRODUCT="$RES/eval_output.txt"

WORK="$RES/.work/eval"
ERR="$WORK/err"
BEST="$WORK/best"

mkdir -p "$RES" "$WORK"

if [ ! -x "$BIN/matmul_mpi" ]; then
    echo "bin/matmul_mpi missing - run 'make' first" >&2
    exit 1
fi

if [ ! -f "$INPUT" ]; then
    echo "no such input file: $INPUT" >&2
    echo "usage: bash scripts/run_eval.sh <input.txt>" >&2
    exit 1
fi

: > "$OUT"
: > "$BEST"
rm -f "$PRODUCT"
echo "procs,rep,scatter,compute,reduce,total" > "$OUTCSV"

say() { echo "$@" | tee -a "$OUT"; }

same_numbers() {
    diff -q <(tr -s ' \n' ' ' < "$1") <(tr -s ' \n' ' ' < "$2") >/dev/null 2>&1
}

say "Q2 - column-row matrix multiplication on a supplied input"
say "file  : $INPUT"
say "date  : $(date)"
say "host  : $(hostname)"
say "reps  : $REPS (fastest of each kept)"
say "procs : $PROCS_LIST"
say ""
say "first line of the file (m and n of A): $(head -1 "$INPUT")"
say ""

disagree=0
failed=0

for P in $PROCS_LIST; do
    best=""

    for rep in $(seq 1 "$REPS"); do
        got="$WORK/p${P}_r${rep}.out"
        $LAUNCH $P "$BIN/matmul_mpi" "$INPUT" -o "$got" --time 2>"$ERR"
        timing=$(cat "$ERR")

        sc=$(echo "$timing" | awk '/^scatter/ {print $2}')
        cp=$(echo "$timing" | awk '/^compute/ {print $2}')
        re=$(echo "$timing" | awk '/^reduce/  {print $2}')
        tt=$(echo "$timing" | awk '/^total/   {print $2}')

        if [ -z "$tt" ] || [ ! -s "$got" ]; then
            say "  P=$P rep=$rep FAILED"
            failed=$((failed + 1))
            rm -f "$got"
            continue
        fi

        sc=${sc:-0}; cp=${cp:-0}; re=${re:-0}

        echo "$P,$rep,$sc,$cp,$re,$tt" >> "$OUTCSV"

        if [ ! -f "$PRODUCT" ]; then
            mv "$got" "$PRODUCT"
        elif ! same_numbers "$got" "$PRODUCT"; then
            disagree=1
            say "  P=$P rep=$rep MISMATCH: product differs from the first run"
            rm -f "$got"
        else
            rm -f "$got"
        fi

        if [ -z "$best" ] || awk "BEGIN{exit !($tt < $best)}"; then
            best=$tt; b_sc=$sc; b_cp=$cp; b_re=$re
        fi
    done

    if [ -n "$best" ]; then
        say "  P=$P  best total ${best}s"
        echo "$P $best $b_sc $b_cp $b_re" >> "$BEST"
    fi
done

say ""
say "==================================================================="
if [ -f "$PRODUCT" ]; then
    rows=$(head -1 "$PRODUCT" | awk '{print NF}')
    lines=$(wc -l < "$PRODUCT" | tr -d ' ')
    say " PRODUCT : $lines x $rows"
    if [ "$lines" -le 20 ]; then
        cat "$PRODUCT" | tee -a "$OUT"
    else
        say " (too large to print - written to $PRODUCT)"
    fi
else
    say " no product - every run failed"
fi
say "==================================================================="

if [ "$disagree" -ne 0 ]; then
    say " WARNING: the product changed with the process count - the timings"
    say "          below cannot be trusted."
fi

if [ -x "$BIN/matmul_seq" ] && [ -f "$PRODUCT" ]; then
    "$BIN/matmul_seq" "$INPUT" -o "$WORK/seq.out" 2>/dev/null
    if [ -s "$WORK/seq.out" ] && same_numbers "$WORK/seq.out" "$PRODUCT"; then
        say " sequential check (bin/matmul_seq): identical product"
    else
        say " sequential check (bin/matmul_seq): DIFFERS"
        disagree=1
    fi
fi

if [ -s "$BEST" ]; then
    say ""
    awk '
    { n++; p[n]=$1; t[n]=$2; sc[n]=$3; cp[n]=$4; re[n]=$5
      if (p[n] == 1) base = t[n] }
    END {
        if (base == 0) base = t[1]

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
               "P", "scatter", "compute", "reduce", "comm %"
        printf "  %-6s %11s %11s %11s %9s\n", \
               "-----", "---------", "---------", "---------", "-------"
        for (i = 1; i <= n; i++) {
            comm = (t[i] > 0) ? 100*(sc[i]+re[i])/t[i] : 0
            printf "  %-6d %11.4f %11.4f %11.4f %8.1f%%\n", \
                   p[i], sc[i], cp[i], re[i], comm
        }
    }' "$BEST" | tee -a "$OUT"
fi

rm -rf "$WORK"

say ""
say "transcript  : $OUT"
say "raw timings : $OUTCSV"
say "product     : $PRODUCT"

if [ "$disagree" -ne 0 ] || [ ! -s "$OUTCSV" ] || [ "$failed" -ne 0 ]; then
    exit 1
fi
exit 0
