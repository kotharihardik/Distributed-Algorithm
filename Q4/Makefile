CXX      ?= g++
MPICXX   ?= mpicxx
CXXFLAGS ?= -O2 -std=c++17 -Wall

# mpirun flags differ by machine
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
LAUNCH   ?= mpirun --oversubscribe -np
else
LAUNCH   ?= mpirun --mca pml ^ucx --mca osc ^ucx --bind-to core -np
endif

# process count for "make run"
P        ?= 4

# the graph handed over on evaluation day, for "make eval"
GRAPH    ?= data/graph.txt

# --hint=nomultithread matters: without it 8 tasks land on 4 real cores
NTASKS   ?= 8
WALL     ?= 01:00:00
PART     ?= debug
SALLOC    = salloc --nodes=1 --ntasks=$(NTASKS) --hint=nomultithread \
                   --time=$(WALL) --partition=$(PART)

BIN = bin
SRC = src

PROGS = $(BIN)/tri_mpi $(BIN)/tri_seq $(BIN)/gen_graph

.PHONY: all run seq test bench eval alloc ctest cbench submit queue graph clean help

all: $(PROGS)

$(BIN):
	mkdir -p $(BIN)

$(BIN)/tri_mpi: $(SRC)/tri_mpi.cpp | $(BIN)
	$(MPICXX) $(CXXFLAGS) -o $@ $<

$(BIN)/tri_seq: $(SRC)/tri_seq.cpp | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ $<

$(BIN)/gen_graph: $(SRC)/gen_graph.cpp | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ $<

## run the worked example (P=$(P))
run: $(PROGS)
	$(LAUNCH) $(P) $(BIN)/tri_mpi data/sample.txt

## same example, sequential
seq: $(PROGS)
	$(BIN)/tri_seq data/sample.txt

## correctness check at P = 1,2,4,8
test: $(PROGS)
	LAUNCH="$(LAUNCH)" bash scripts/run_correctness.sh

## timing sweep, writes results/
bench: $(PROGS)
	LAUNCH="$(LAUNCH)" bash scripts/run_benchmark.sh

## one supplied graph at P = 1,2,4,8   (make eval GRAPH=graph.txt)
eval: $(PROGS)
	LAUNCH="$(LAUNCH)" bash scripts/run_eval.sh $(GRAPH)

## grab cores and drop into a shell
alloc:
	$(SALLOC)

## allocate + correctness + release, one command
ctest:
	$(SALLOC) $(MAKE) test

## allocate + benchmark + release, one command
cbench:
	$(SALLOC) $(MAKE) bench

## hand the whole job to SLURM
submit:
	sbatch scripts/job_q4.slurm

## show my jobs in the queue
queue:
	squeue -u $(USER)

## redraw the speed-up graph
graph:
	@test -f scripts/plot_speedup.py || { echo "scripts/plot_speedup.py not present"; exit 1; }
	python3 scripts/plot_speedup.py results/benchmark_raw.csv results/speedup.png

## remove binaries and generated data
clean:
	rm -rf $(BIN) data/generated results/.work scripts/__pycache__

help:
	@echo "Q4 - Triangle counting in an undirected graph"
	@echo ""
	@echo "  make            compile everything into bin/"
	@echo "  make run        run the paper's example      (P=$(P), change with P=8)"
	@echo "  make seq        same example, sequential version"
	@echo "  make test       correctness check at P = 1,2,4,8"
	@echo "  make bench      timing sweep, writes results/"
	@echo "  make eval       one supplied graph at P = 1,2,4,8"
	@echo "                  e.g. make eval GRAPH=graph.txt"
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
