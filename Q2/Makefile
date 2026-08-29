# Q2 - Matrix multiplication, Column-Row method

CXX      ?= g++
MPICXX   ?= mpicxx
CXXFLAGS ?= -O2 -std=c++17 -Wall

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
LAUNCH   ?= mpirun --oversubscribe -np
else
LAUNCH   ?= mpirun --mca pml ^ucx --mca osc ^ucx --bind-to core -np
endif

P ?= 4

BIN = bin
SRC = src

PROGS = $(BIN)/matmul_mpi $(BIN)/matmul_seq $(BIN)/gen_matrix

.PHONY: all run seq test bench submit queue graph clean help

all: $(PROGS)

$(BIN):
	mkdir -p $(BIN)

$(BIN)/matmul_mpi: $(SRC)/matmul_mpi.cpp | $(BIN)
	$(MPICXX) $(CXXFLAGS) -o $@ $<

$(BIN)/matmul_seq: $(SRC)/matmul_seq.cpp | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ $<

$(BIN)/gen_matrix: $(SRC)/gen_matrix.cpp | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ $<

run: $(PROGS)
	$(LAUNCH) $(P) $(BIN)/matmul_mpi data/example1.txt

seq: $(PROGS)
	$(BIN)/matmul_seq data/example1.txt

test: $(PROGS)
	LAUNCH="$(LAUNCH)" bash scripts/run_correctness.sh

bench: $(PROGS)
	LAUNCH="$(LAUNCH)" bash scripts/run_benchmark.sh

submit:
	sbatch scripts/job_q2.slurm

queue:
	squeue -u $(USER)

graph:
	@test -f scripts/plot_speedup.py || { echo "scripts/plot_speedup.py is not in this tree - results/speedup.png is already built"; exit 1; }
	python3 scripts/plot_speedup.py results/benchmark_raw.csv results/speedup.png

clean:
	rm -rf $(BIN) data/generated results/.work scripts/__pycache__

help:
	@echo "Q2 - Matrix multiplication, Column-Row method"
	@echo ""
	@echo "  make            compile everything into bin/"
	@echo "  make run        run the paper's example      (P=$(P), change with P=8)"
	@echo "  make seq        same example, sequential version"
	@echo "  make test       correctness check at P = 1,2,4,8"
	@echo "  make bench      timing sweep, writes results/"
	@echo "  make submit     sbatch the whole thing on the cluster"
	@echo "  make queue      squeue -u $(USER)"
	@echo "  make graph      redraw results/speedup.png"
	@echo "  make clean      remove binaries and generated data"
	@echo ""
	@echo "On the cluster first:  module purge && module load openmpi/4.1.5"
