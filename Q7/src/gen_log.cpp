#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <cstdlib>

#define ll long long
using namespace std;

// Pick a random number from 0 to n-1.
ll bounded(ll n){
    return rand() % n;
}

// Generate values that are more likely to be near the beginning of the range.
ll skewed(ll range){
    ll hot = range / 5;

    if(hot < 1)
        hot = 1;

    if(bounded(100) < 70)
        return bounded(hot);

    return bounded(range);
}

int main(int argc, char **argv){
    if(argc < 6){
        cerr << "usage: " << argv[0] << " <N> <K> <S> <seed> <outfile> [endpoints]\n"
             << "  N          number of log lines\n"
             << "  K          size of each top-K list\n"
             << "  S          number of servers (ids 0..S-1)\n"
             << "  seed       fixes the output; same seed, same file\n"
             << "  endpoints  distinct endpoint ids, default 200" << endl;
        return 1;
    }

    ll N = stoll(argv[1]);
    ll K = stoll(argv[2]);
    ll S = stoll(argv[3]);

    unsigned seed = (unsigned)stoul(argv[4]);

    string outPath = argv[5];

    //we are using 200 endpoints if the user does not provide one.
    ll E;
    if(argc > 6){
        E = stoll(argv[6]);
    }
    else{
        E = 200;
    }

    if(N < 1 || K < 1 || S < 1 || E < 1){
        cerr << "N, K, S and endpoints must all be positive" << endl;
        return 1;
    }

    srand(seed);

    ofstream fout(outPath.c_str());
    if(!fout){
        cerr << "cannot write " << outPath << endl;
        return 1;
    }

    fout << fixed << setprecision(1);

    fout << N << " " << K << " " << S << "\n";

    ll ts = 1700000000;

    for(ll i = 0; i < N; i++){

        ts += bounded(4);

        ll server = skewed(S);
        ll endpoint = skewed(E);
        ll user = bounded(100000);

        ll roll = bounded(100);
        ll status;

        if(roll < 85)
            status = 200 + bounded(3);       
        else if(roll < 90)
            status = 301 + bounded(2);      
        else if(roll < 97)
            status = 400 + bounded(5);       
        else
            status = 500 + bounded(4);       

        double response = (1 + bounded(20000)) / 10.0;
        ll bytes = 100 + bounded(60000);

        fout << ts << " " << server << " " << endpoint << " " << user << " " << status << " " << response << " " << bytes << "\n";
    }

    fout.close();

    cerr << "wrote " << outPath
         << " (N=" << N << ", K=" << K << ", S=" << S << ", endpoints=" << E << ", seed=" << seed << ")" << endl;

    return 0;
}
