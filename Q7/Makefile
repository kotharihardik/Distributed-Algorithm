# Q7 - Large-scale server log analytics
#
# Short commands instead of long ones. Run "make help" for the list.
#
# On the cluster, load the MPI module first:
#     module purge && module load openmpi/4.1.5

CXX      ?= g++
MPICXX   ?= mpicxx
CXXFLAGS ?= -O2 -std=c++17 -Wall

# How MPI programs get launched, and it has to differ by machine.
#
# On the cluster: the ucx flags silence a missing-library warning, and
# --bind-to core pins each rank so the OS cannot shuffle them around.
#
# On a Mac: macOS exposes no processor binding at all, so --bind-to core is a
# hard error rather than a warning; and a laptop has fewer cores than we want
# ranks, so oversubscribing has to be allowed.
#
# Override it by hand if needed:  make test LAUNCH="mpirun -np"
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
LAUNCH   ?= mpirun --oversubscribe -np
else
LAUNCH   ?= mpirun --mca pml ^ucx --mca osc ^ucx --bind-to core -np
endif

# Process count used by "make run"
P        ?= 4

# What to ask SLURM for. Used by alloc / ctest / cbench below.
#   --hint=nomultithread is not optional: the nodes are 24 real cores shown as
#   48 hyperthreads, and without it 8 tasks land on 4 real cores and P=8 comes
#   out half as fast, silently.
NTASKS   ?= 8
WALL     ?= 01:00:00
PART     ?= debug
SALLOC    = salloc --nodes=1 --ntasks=$(NTASKS) --hint=nomultithread \
                   --time=$(WALL) --partition=$(PART)

BIN = bin
SRC = src

PROGS = $(BIN)/log_mpi $(BIN)/log_seq $(BIN)/gen_log

.PHONY: all run seq test bench alloc ctest cbench submit queue graph clean help

all: $(PROGS)

$(BIN):
	mkdir -p $(BIN)

$(BIN)/log_mpi: $(SRC)/log_mpi.cpp | $(BIN)
	$(MPICXX) $(CXXFLAGS) -o $@ $<

$(BIN)/log_seq: $(SRC)/log_seq.cpp | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ $<

$(BIN)/gen_log: $(SRC)/gen_log.cpp | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ $<

## run the worked example from the question paper (P=$(P))
run: $(PROGS)
	$(LAUNCH) $(P) $(BIN)/log_mpi data/sample.txt

## same example through the sequential reference
seq: $(PROGS)
	$(BIN)/log_seq data/sample.txt

## full correctness check at P = 1, 2, 4, 8
test: $(PROGS)
	LAUNCH="$(LAUNCH)" bash scripts/run_correctness.sh

## timing sweep over all four input sizes
bench: $(PROGS)
	LAUNCH="$(LAUNCH)" bash scripts/run_benchmark.sh

## grab cores and drop into a shell (then run: make test / make bench)
alloc:
	$(SALLOC)

## allocate, run the correctness check, release the cores - one command
ctest:
	$(SALLOC) $(MAKE) test

## allocate, run the benchmark, release the cores - one command
cbench:
	$(SALLOC) $(MAKE) bench

## hand the whole job to SLURM
submit:
	sbatch scripts/job_q7.slurm

## show my jobs in the queue
queue:
	squeue -u $(USER)

## redraw the speed-up graph from existing results (needs the analysis script)
graph:
	@test -f scripts/plot_speedup.py || { echo "scripts/plot_speedup.py is not in this tree - results/speedup.png is already built"; exit 1; }
	python3 scripts/plot_speedup.py results/benchmark_raw.csv results/speedup.png

## delete binaries and generated data (keeps results)
clean:
	rm -rf $(BIN) data/generated results/.work scripts/__pycache__

help:
	@echo "Q7 - Large-scale server log analytics"
	@echo ""
	@echo "  make            compile everything into bin/"
	@echo "  make run        run the paper's example      (P=$(P), change with P=8)"
	@echo "  make seq        same example, sequential version"
	@echo "  make test       correctness check at P = 1,2,4,8"
	@echo "  make bench      timing sweep, writes results/"
	@echo ""
	@echo "  on the cluster - these ask SLURM for cores first:"
	@echo "  make ctest      allocate + correctness + release"
	@echo "  make cbench     allocate + benchmark + release"
	@echo "  make alloc      just grab a shell with cores"
	@echo "  make submit     sbatch the whole thing on the cluster"
	@echo "  make queue      squeue -u $(USER)"
	@echo "  make graph      redraw results/speedup.png"
	@echo "  make clean      remove binaries and generated data"
	@echo ""
	@echo "On the cluster first:  module purge && module load openmpi/4.1.5"
