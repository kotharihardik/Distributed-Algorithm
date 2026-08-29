#!/bin/bash
#SBATCH --job-name=q4-tricount
#SBATCH --nodes=1
#SBATCH --ntasks=8
#SBATCH --cpus-per-task=1
#SBATCH --hint=nomultithread
#SBATCH --mem=16G
#SBATCH --time=02:00:00
#SBATCH --partition=debug
#SBATCH --output=results/slurm_%j.log
#SBATCH --error=results/slurm_%j.err

# Q4 - one job that builds, checks correctness, then runs the full timing sweep.
# Submit from the Q4 directory:
#     sbatch scripts/job_q4.slurm
#
# Why a single node with 8 tasks rather than 2 nodes x 4:
#   Each compute node here has 40 cores, so 8 ranks fit comfortably on one and
#   every rank still gets a core to itself. Multi-node runs on this cluster do
#   not work - hpcx mpirun aborts with "munmap_chunk(): invalid pointer", srun
#   fails in MPI_Init, and openmpi/4.1.5 hangs on the interconnect. Staying on
#   one node sidesteps all of that and the scaling measurement is unaffected.

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

# openmpi/4.1.5 rather than hpcx: hpcx's launcher crashes here.
module purge 2>/dev/null
module load openmpi/4.1.5

# UCX is advertised but libucp.so.0 is missing on the nodes, so the ucx
# components must be turned off or every rank prints a load failure.
# --hint=nomultithread above and --bind-to core here matter a great deal: the
# nodes are 24 physical cores presented as 48 hyperthreads, so without them
# eight ranks land on four real cores and the compute time at P=8 comes out
# identical to P=4. With them it halves, as it should.
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
