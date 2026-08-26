#include <iostream>
#include <fstream>
#include <string>
#include <random>

using namespace std;

int main(int argc, char **argv)
{
    if(argc < 6){
        cerr << "usage: " << argv[0] << " <m> <n> <p> <seed> <outfile> [maxabs]\n"
             << "  maxabs defaults to 9 (entries drawn from -maxabs..maxabs)" << endl;
        return 1;
    }

    int m = stoi(argv[1]);
    int n = stoi(argv[2]);
    int p = stoi(argv[3]);

    unsigned seed = (unsigned)stoul(argv[4]);

    string outPath = argv[5];

    int maxabs;
    if(argc > 6){
        maxabs = stoi(argv[6]);
    }
    else{
        maxabs = 9;
    }

    if(m <= 0 || n <= 0 || p <= 0 || maxabs <= 0){
        cerr << "dimensions and maxabs must be positive" << endl;
        return 1;
    }

    ofstream fout(outPath.c_str());
    if(!fout){
        cerr << "cannot write " << outPath << endl;
        return 1;
    }

    mt19937 rng(seed);

    long long span = 2LL * maxabs + 1;
    long long bucket = (1ULL << 32) / span;
    long long cutoff = bucket * span;

    fout << m << " " << n << " " << p << "\n";

    for(int i = 0; i < m; i++){
        for(int j = 0; j < n; j++){

            int value = 0;
            while(true){
                long long r = rng();

                if(r < cutoff){
                    value = (int)(r / bucket) - maxabs;
                    break;
                }
            }

            fout << value;
            if(j + 1 == n)
                fout << "\n";
            else
                fout << " ";
        }
    }

    for(int i = 0; i < n; i++){
        for(int j = 0; j < p; j++){

            int value = 0;

            while(true){
                long long r = rng();

                if(r < cutoff){
                    value = (int)(r / bucket) - maxabs;
                    break;
                }
            }

            fout << value;
            if(j + 1 == p)
                fout << "\n";
            else
                fout << " ";
        }
    }

    fout.close();
    cerr << "wrote " << outPath << "  (A " << m << "x" << n << ", B " << n << "x" << p << ", seed " << seed << ")" << endl;

    return 0;
}

// Build : g++ -O2 -std=c++17 -o gen_matrix gen_matrix.cpp
// Run   : ./gen_matrix 1000 1000 1000 12345 data/large.txt