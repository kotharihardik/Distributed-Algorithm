#!/bin/bash
#SBATCH --job-name=q7-loganalytics
#SBATCH --nodes=1
#SBATCH --ntasks=8
#SBATCH --cpus-per-task=1
#SBATCH --hint=nomultithread
#SBATCH --mem=16G
#SBATCH --time=02:00:00
#SBATCH --partition=debug
#SBATCH --output=results/slurm_%j.log
#SBATCH --error=results/slurm_%j.err


set -u

cd "$SLURM_SUBMIT_DIR"
mkdir -p results

echo "========================================="
echo "job id    : $SLURM_JOB_ID"
echo "nodes     : $SLURM_NNODES"
echo "tasks     : $SLURM_NTASKS"
echo "node list : $SLURM_NODELIST"
echo "started   : $(date)"
echo "========================================="
echo

module purge 2>/dev/null
module load openmpi/4.1.5

export LAUNCH="mpirun --mca pml ^ucx --mca osc ^ucx --bind-to core -np"

echo ">> building"
make || { echo "build failed"; exit 1; }
echo

echo ">> correctness"
bash scripts/run_correctness.sh
echo

echo ">> benchmark"
bash scripts/run_benchmark.sh
echo

echo "========================================="
echo "finished  : $(date)"
echo "results in $SLURM_SUBMIT_DIR/results/"
echo "========================================="
