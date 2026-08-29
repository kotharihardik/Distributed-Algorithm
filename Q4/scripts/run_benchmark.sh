#!/bin/bash
#
# Q4 - timing sweep.  How much faster does tri_mpi get with more processes?
#
# Correctness is assumed here; run_correctness.sh checks that separately.
#
# Four graph sizes, each run at P = 1, 2, 4 and 8. Every run is repeated and
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
SEED="${SEED:-12345}"            # fixes which graphs get generated

mkdir -p "$RES" "$DATA/generated"

CSV="$RES/benchmark_raw.csv"
LOG="$RES/benchmark_log.txt"

if [ ! -x "$BIN/tri_mpi" ]; then
    echo "bin/tri_mpi missing - run 'make' first" >&2
    exit 1
fi


# ---------------------------------------------------------------------------
# The four sizes that appear in the report, and nothing else.
#
# Format is  label:V:E
#
# Both the edge count and the density climb across the four, because the work
# a triangle count actually does is roughly E^2/V - a graph with twice the
# edges but twice the vertices is barely harder. Very large sits at the
# question paper's ceiling of E = 10^6, and every size stays inside the stated
# limits of V <= 10^5 and E <= 10^6.
# ---------------------------------------------------------------------------
CASES="
Small:20000:250000
Medium:10000:500000
Large:6000:750000
Verylarge:4000:1000000
"


# ---------------------------------------------------------------------------
# Set up the two output files.
#
# The CSV keeps every individual run so nothing is lost; the log is the
# readable version a human would actually scroll through.
# ---------------------------------------------------------------------------
echo "label,V,E,procs,rep,read,bcast,build,count,reduce,total" > "$CSV"
{
    echo "Q4 triangle counting benchmark"
    echo "started : $(date)"
    echo "host    : $(hostname)"
    echo "reps    : $REPS (fastest kept)"
    echo "seed    : $SEED"
    echo
} > "$LOG"


# ---------------------------------------------------------------------------
# Main sweep: for each graph size, for each process count, time it.
# ---------------------------------------------------------------------------
for case in $CASES; do
    label=$(echo "$case" | cut -d: -f1)
    Vv=$(echo "$case" | cut -d: -f2)
    Ee=$(echo "$case" | cut -d: -f3)

    # Make the graph the first time only. Reusing the same file afterwards is
    # what lets two separate benchmark runs be compared against each other.
    src="$DATA/generated/${label}.txt"
    if [ ! -f "$src" ]; then
        echo "generating $label (V=$Vv E=$Ee) ..." | tee -a "$LOG"
        "$BIN/gen_graph" "$Vv" "$Ee" "$SEED" "$src" 2>>"$LOG"
    fi

    echo "" | tee -a "$LOG"
    echo "--- $label  V=$Vv  E=$Ee ---" | tee -a "$LOG"

    for P in 1 2 4 8; do
        best=""

        for rep in $(seq 1 "$REPS"); do
            # --time makes the program print its phase timings on stderr. The
            # triangle count itself goes to stdout and is thrown away here.
            timing=$($LAUNCH $P "$BIN/tri_mpi" "$src" --time 2>&1 >/dev/null)

            # Pull one number out of each line of that report.
            rd=$(echo "$timing" | awk '/^read/   {print $2}')   # rank 0 reads the file
            bc=$(echo "$timing" | awk '/^bcast/  {print $2}')   # send edges everywhere
            bd=$(echo "$timing" | awk '/^build/  {print $2}')   # build adjacency lists
            ct=$(echo "$timing" | awk '/^count/  {print $2}')   # count triangles
            re=$(echo "$timing" | awk '/^reduce/ {print $2}')   # add the counts up
            tt=$(echo "$timing" | awk '/^total/  {print $2}')

            if [ -z "$tt" ]; then
                echo "  P=$P rep=$rep FAILED" | tee -a "$LOG"
                continue
            fi

            # A phase that reported nothing counts as zero, not a blank field,
            # otherwise the summariser chokes on an empty column later.
            rd=${rd:-0}; bc=${bc:-0}; bd=${bd:-0}; ct=${ct:-0}; re=${re:-0}

            echo "$label,$Vv,$Ee,$P,$rep,$rd,$bc,$bd,$ct,$re,$tt" >> "$CSV"

            # Keep this run if it beat the previous best. awk does the compare
            # because the shell cannot compare decimals.
            if [ -z "$best" ] || awk "BEGIN{exit !($tt < $best)}"; then
                best=$tt; b_bc=$bc; b_bd=$bd; b_ct=$ct; b_re=$re
            fi
        done

        if [ -n "$best" ]; then
            printf "  P=%-2s total %8.4fs   bcast %7.4f  build %7.4f  count %7.4f  reduce %7.4f\n" \
                   "$P" "$best" "$b_bc" "$b_bd" "$b_ct" "$b_re" | tee -a "$LOG"
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
