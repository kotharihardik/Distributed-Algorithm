#!/bin/bash
#
# Q2 - timing sweep.  How much faster does matmul_mpi get with more processes?
#
# Correctness is assumed here; run_correctness.sh checks that separately.
#
# Four matrix sizes, each run at P = 1, 2, 4 and 8. Every run is repeated and
# only the fastest kept, because the cluster is shared and an unlucky run says
# more about the other jobs on the node than about this program.
#
#   make bench
#   REPS=5 bash scripts/run_benchmark.sh
#
# Writes into results/:  benchmark_raw.csv, benchmark_log.txt,
#                        report_tables.md, speedup.png

set -u

cd "$(dirname "$0")/.."

BIN=bin
DATA=data
RES=results

LAUNCH="${LAUNCH:-mpirun -np}"   # Makefile supplies the machine's flags
REPS="${REPS:-3}"                # runs per measurement, fastest wins
SEED="${SEED:-12345}"            # fixes which matrices get generated

mkdir -p "$RES" "$DATA/generated"

CSV="$RES/benchmark_raw.csv"
LOG="$RES/benchmark_log.txt"

if [ ! -x "$BIN/matmul_mpi" ]; then
    echo "bin/matmul_mpi missing - run 'make' first" >&2
    exit 1
fi


# ---------------------------------------------------------------------------
# The four sizes that appear in the report, and nothing else.
#
# Format is  label:m:n:p   (A is m x n, B is n x p)
#
# The question paper puts the timing range at "1000 x 1000 or more", so Medium
# sits exactly on that figure and the two larger sizes go above it. Small is
# below the threshold on purpose: at n = 500 the run finishes before MPI has
# finished starting up, which is what makes the communication-vs-computation
# table worth reading.
# ---------------------------------------------------------------------------
CASES="
Small:500:500:500
Medium:1000:1000:1000
Large:2000:2000:2000
Verylarge:4000:4000:4000
"


# ---------------------------------------------------------------------------
# Set up the two output files.
#
# The CSV keeps every individual run so nothing is lost; the log is the
# readable version a human would actually scroll through.
# ---------------------------------------------------------------------------
echo "label,m,n,p,procs,rep,scatter,compute,reduce,total" > "$CSV"
{
    echo "Q2 column-row matrix multiplication benchmark"
    echo "started : $(date)"
    echo "host    : $(hostname)"
    echo "reps    : $REPS (fastest kept)"
    echo "seed    : $SEED"
    echo
} > "$LOG"


# ---------------------------------------------------------------------------
# Main sweep: for each matrix size, for each process count, time it.
# ---------------------------------------------------------------------------
for case in $CASES; do
    label=$(echo "$case" | cut -d: -f1)
    m=$(echo "$case" | cut -d: -f2)
    n=$(echo "$case" | cut -d: -f3)
    p=$(echo "$case" | cut -d: -f4)

    # Make the input the first time only. Reusing the same file afterwards is
    # what lets two separate benchmark runs be compared against each other.
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
            # --time makes the program print its phase timings on stderr. The
            # triangle count itself goes to stdout and is thrown away here.
            timing=$($LAUNCH $P "$BIN/matmul_mpi" "$src" -o /dev/null --time 2>&1 >/dev/null)

            # Pull one number out of each line of that report.
            sc=$(echo "$timing" | awk '/^scatter/ {print $2}')  # hand out the pairs
            cp=$(echo "$timing" | awk '/^compute/ {print $2}')  # the outer products
            re=$(echo "$timing" | awk '/^reduce/  {print $2}')  # sum the partial C
            tt=$(echo "$timing" | awk '/^total/   {print $2}')

            if [ -z "$tt" ]; then
                echo "  P=$P rep=$rep FAILED" | tee -a "$LOG"
                continue
            fi

            # A phase that reported nothing counts as zero, not a blank field,
            # otherwise the summariser chokes on an empty column later.
            sc=${sc:-0}; cp=${cp:-0}; re=${re:-0}

            echo "$label,$m,$n,$p,$P,$rep,$sc,$cp,$re,$tt" >> "$CSV"

            # Keep this run if it beat the previous best. awk does the compare
            # because the shell cannot compare decimals.
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


# ---------------------------------------------------------------------------
# Turn the raw numbers into the report tables, and the graph if this machine
# can draw one. The cluster has no matplotlib, so that step is optional and
# the graph gets drawn later on a laptop instead.
# ---------------------------------------------------------------------------
# Both are optional extras. The timings above are the real output; these two
# just format them. They are not part of the submitted tree, so check they are
# actually here before calling them.
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
