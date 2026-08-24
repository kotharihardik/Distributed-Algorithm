// Q2 - Distributed Matrix Multiplication : Column-Row (outer product) method

#include <mpi.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>

using namespace std;

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);

    int rank, nprocs;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

    string inPath = "";
    string outPath = "";
    bool show_time = false;

    for(int i = 1; i < argc; i++){
        string arg = argv[i];

        if(arg == "-o" && i+1 < argc){
            outPath = argv[i+1];
            i++;
        }
        else if(arg == "--time"){
            show_time = true;
        }
        else if(inPath == ""){
            inPath = arg;
        }
    }

    if(inPath == ""){
        if(rank == 0)
            cerr << "usage: " << argv[0] << " <input> [-o output] [--time]" << endl;
        MPI_Finalize();
        return 1;
    }


    int m = 0, n = 0, p = 0;
    bool ok = true;

    vector<int> Acolumns;   
    vector<int> Brows;       

    if(rank == 0){
        ifstream fin(inPath.c_str());

        if(!fin){
            cerr << "cannot open " << inPath << endl;
            ok = false;
        }
        else{
            fin >> m >> n >> p;

            if(fin.fail() || m <= 0 || n <= 0 || p <= 0){
                cerr << "bad header in " << inPath << endl;
                ok = false;
            }
            else{
                Acolumns.assign((size_t)n * m, 0);
                Brows.assign((size_t)n * p, 0);

                for(int i = 0; i < m; i++){
                    for(int k = 0; k < n; k++){
                        fin >> Acolumns[(size_t)k * m + i];   
                    }
                }

                for(int k = 0; k < n; k++){
                    for(int j = 0; j < p; j++){
                        fin >> Brows[(size_t)k * p + j];
                    }
                }

                if(fin.fail()){
                    cerr << "ran out of values in " << inPath << endl;
                    ok = false;
                }
            }
        }

        if(!ok) m = -1;      // signal the failure to everyone else
    }

    // Everybody must learn about a bad input, or the collectives below hang.
    int dims[3] = { m, n, p };
    MPI_Bcast(dims, 3, MPI_INT, 0, MPI_COMM_WORLD);
    m = dims[0];
    n = dims[1];
    p = dims[2];

    if(m < 0){
        MPI_Finalize();
        return 1;
    }


    vector<int> pairCount(nprocs);
    vector<int> pairStart(nprocs);

    int base = n / nprocs;
    int rem = n % nprocs;
    int at = 0;

    for(int r = 0; r < nprocs; r++){
        pairCount[r] = base + (r < rem ? 1 : 0);
        pairStart[r] = at;
        at += pairCount[r];
    }

    int myPairs = pairCount[rank];

    vector<int> sendA(nprocs), dispA(nprocs);
    vector<int> sendB(nprocs), dispB(nprocs);

    for(int r = 0; r < nprocs; r++){
        sendA[r] = pairCount[r] * m;
        dispA[r] = pairStart[r] * m;
        sendB[r] = pairCount[r] * p;
        dispB[r] = pairStart[r] * p;
    }

    vector<int> myA((size_t)myPairs * m, 0);
    vector<int> myB((size_t)myPairs * p, 0);

    MPI_Barrier(MPI_COMM_WORLD);
    double t_start = MPI_Wtime();

    if(rank == 0){
        MPI_Scatterv(Acolumns.data(), sendA.data(), dispA.data(), MPI_INT,
                     myA.data(), sendA[rank], MPI_INT, 0, MPI_COMM_WORLD);

        MPI_Scatterv(Brows.data(), sendB.data(), dispB.data(), MPI_INT,
                     myB.data(), sendB[rank], MPI_INT, 0, MPI_COMM_WORLD);
    }
    else{
        MPI_Scatterv(NULL, sendA.data(), dispA.data(), MPI_INT,
                     myA.data(), sendA[rank], MPI_INT, 0, MPI_COMM_WORLD);

        MPI_Scatterv(NULL, sendB.data(), dispB.data(), MPI_INT,
                     myB.data(), sendB[rank], MPI_INT, 0, MPI_COMM_WORLD);
    }
    double t_scattered = MPI_Wtime();

    vector<long long> local((size_t)m * p, 0);

    for(int k = 0; k < myPairs; k++){
        for(int i = 0; i < m; i++){
            long long a = myA[(size_t)k * m + i];

            if(a == 0) continue;            

            for(int j = 0; j < p; j++){
                local[(size_t)i * p + j] += a * myB[(size_t)k * p + j];
            }
        }
    }

    double t_computed = MPI_Wtime();

    // reduce : sum the partial matrices onto the master 
    vector<long long> C;
    if(rank == 0)   
        C.assign((size_t)m * p, 0);

    if(rank == 0){
        MPI_Reduce(local.data(), C.data(),
                m * p, MPI_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
    }
    else{
        MPI_Reduce(local.data(), NULL,
                m * p, MPI_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
    }
    double t_end = MPI_Wtime();

    // the master writes the result 
    if(rank == 0){
        ofstream fout;
        if(outPath != ""){
            fout.open(outPath.c_str());

            if(!fout){
                cerr << "cannot write " << outPath << endl;
                MPI_Abort(MPI_COMM_WORLD, 1);
            }
        }

        ostream *out = &cout;
        if(outPath != "") out = &fout;

        for(int i = 0; i < m; i++){
            for(int j = 0; j < p; j++){
                
                *out << C[(size_t)i * p + j];
                if(j + 1 == p)
                    *out << "\n";
                else
                    *out << " ";
            }
        }

        if(outPath != "") fout.close();
    }

    //  timing 
    if(show_time){
        // Report the slowest process in each phase - that is what the run costs.
        double mine[4];
        mine[0] = t_scattered - t_start;
        mine[1] = t_computed - t_scattered;
        mine[2] = t_end - t_computed;
        mine[3] = t_end - t_start;

        double worst[4];
        MPI_Reduce(mine, worst, 4, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

        if(rank == 0){
            cerr.setf(ios::fixed);
            cerr.precision(6);
            cerr << "procs " << nprocs << "  m " << m << "  n " << n << "  p " << p << endl;
            cerr << "scatter  " << worst[0] << endl;
            cerr << "compute  " << worst[1] << endl;
            cerr << "reduce   " << worst[2] << endl;
            cerr << "total    " << worst[3] << endl;
        }
    }

    MPI_Finalize();
    return 0;
}

// Build : mpicxx -O2 -std=c++17 -o matmul_mpi matmul_mpi.cpp
// Run   : mpirun -np 4 ./matmul_mpi input.txt -o result.txt