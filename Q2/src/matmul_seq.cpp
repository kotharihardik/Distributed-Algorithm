// Q2 - Column-Row (outer product) method : sequential reference

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <chrono>

using namespace std;

int main(int argc, char **argv)
{
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
        cerr << "usage: " << argv[0] << " <input> [-o output] [--time]" << endl;
        return 1;
    }

    ifstream fin(inPath.c_str());
    if(!fin){
        cerr << "cannot open " << inPath << endl;
        return 1;
    }

    int m, n, p;
    fin >> m >> n >> p;
    if(fin.fail() || m <= 0 || n <= 0 || p <= 0){
        cerr << "bad header in " << inPath << endl;
        return 1;
    }

    vector<vector<int> > Acolumns(n, vector<int>(m , 0));
    for(int i = 0; i < m; i++) {
        for(int k = 0; k < n; k++) {
            fin >> Acolumns[k][i];
            if(fin.fail()) {
                cerr << "A: ran out of values" << endl;
                return 1;
            }
        }
    }


    vector<vector<int> > Brows(n, vector<int>(p , 0));
    for(int k = 0; k < n; k++){
        for(int j = 0; j < p; j++){
            fin >> Brows[k][j];
            if(fin.fail()){
                cerr << "B: ran out of values" << endl;
                return 1;
            }
        }
    }

    fin.close();

    chrono::steady_clock::time_point t0 = chrono::steady_clock::now();
    vector<vector<long long> > C(m, vector<long long>(p, 0));

    for(int k = 0; k < n; k++){
        for(int i = 0; i < m; i++){
            long long a = Acolumns[k][i];
            
            if (a == 0) continue;            
            
            for(int j = 0; j < p; j++){
                C[i][j] += a * Brows[k][j];
            }
        }
    }

    chrono::steady_clock::time_point t1 = chrono::steady_clock::now();


    ofstream fout;
    if(outPath != ""){
        fout.open(outPath.c_str());

        if(!fout){
            cerr << "cannot write " << outPath << endl;
            return 1;
        }
    }

    ostream &out = (outPath != "") ? (ostream &)fout : cout;

    for(int i = 0; i < m; i++){
        for(int j = 0; j < p; j++){
            out << C[i][j];
            if(j + 1 == p)
                out << "\n";
            else 
                out << " ";
        }
    }
    if(outPath != "") fout.close();

    // ---- timing ----
    if(show_time){
        double secs = chrono::duration<double>(t1 - t0).count();
        cerr.setf(ios::fixed);
        cerr.precision(6);
        cerr << "procs 1  m " << m << "  n " << n << "  p " << p << endl;
        cerr << "compute  " << secs << endl;
        cerr << "total    " << secs << endl;
    }

    return 0;
}

// Build : g++ -O2 -std=c++17 -o matmul_seq matmul_seq.cpp
// Run   : ./matmul_seq input.txt -o result.txt