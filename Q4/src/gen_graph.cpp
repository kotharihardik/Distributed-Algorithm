/*
 * Test-graph generator for Q4.
 *
 * Writes a file in the format the solvers expect:
 *      V E
 *      E lines of "u v"
 *
 * The graph is a uniform random one on V vertices with exactly E distinct
 * undirected edges, no self-loops. mt19937 with an explicit seed means the
 * same command always writes the same file, byte for byte - which is what
 * makes the benchmark numbers reproducible.
 *
 * Duplicates are removed by generating a surplus, sorting and de-duplicating,
 * then topping up if the trim left us short. Rejecting into a hash set edge by
 * edge would work too, but it slows down badly once the graph gets dense and
 * most candidates are already present.
 *
 * Note the hand-written bounded() and the hand-written shuffle. The standard
 * uniform_int_distribution and std::shuffle are NOT specified down to the
 * exact value they return, so libstdc++ on the cluster and libc++ on a Mac
 * hand back different sequences from the same seed - which showed up here as
 * the same command producing two different graphs. Only mt19937's raw output
 * is pinned by the standard, so everything below is built from that.
 *
 * Build : g++ -O2 -std=c++17 -o gen_graph gen_graph.cpp
 * Run   : ./gen_graph 10000 1000000 12345 data/large.txt
 */

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>

using namespace std;

int main(int argc, char **argv)
{
    if (argc < 5) {
        fprintf(stderr,
                "usage: %s <V> <E> <seed> <outfile>\n"
                "  V vertices, E distinct undirected edges, 0-indexed\n",
                argv[0]);
        return 1;
    }

    long long V = atoll(argv[1]);
    long long E = atoll(argv[2]);
    unsigned seed = (unsigned)strtoul(argv[3], nullptr, 10);
    const char *path = argv[4];

    if (V < 3 || E < 1) {
        fprintf(stderr, "need V >= 3 and E >= 1\n");
        return 1;
    }

    long long max_edges = V * (V - 1) / 2;
    if (E > max_edges) {
        fprintf(stderr, "E=%lld is more than the %lld edges %lld vertices allow\n",
                E, max_edges, V);
        return 1;
    }

    mt19937 rng(seed);

    /* unbiased draw from [0, n), using only the generator's raw output */
    auto bounded = [&rng](unsigned long long n) -> unsigned long long {
        unsigned long long bucket = (1ULL << 32) / n;
        unsigned long long cutoff = bucket * n;
        for (;;) {
            unsigned long long r = rng();
            if (r < cutoff) return r / bucket;
        }
    };

    /* one 64-bit key per edge, small endpoint first so u-v and v-u collide */
    vector<long long> keys;
    keys.reserve((size_t)(E * 1.15) + 16);

    auto draw = [&](long long want) {
        for (long long i = 0; i < want; i++) {
            long long a = (long long)bounded(V), b = (long long)bounded(V);
            if (a == b) continue;
            if (a > b) swap(a, b);
            keys.push_back(a * V + b);
        }
    };

    draw((long long)(E * 1.15) + 16);

    sort(keys.begin(), keys.end());
    keys.erase(unique(keys.begin(), keys.end()), keys.end());

    /* dedup may have left us short on a dense graph - top up and retry */
    int rounds = 0;
    while ((long long)keys.size() < E && rounds++ < 200) {
        long long missing = E - (long long)keys.size();
        draw(missing * 2 + 64);
        sort(keys.begin(), keys.end());
        keys.erase(unique(keys.begin(), keys.end()), keys.end());
    }

    if ((long long)keys.size() < E) {
        fprintf(stderr, "could not reach %lld distinct edges (got %zu)\n",
                E, keys.size());
        return 1;
    }
    keys.resize(E);

    /* sorted keys would hand the solver a suspiciously tidy file.
       Fisher-Yates by hand, for the portability reason noted above. */
    for (size_t i = keys.size(); i > 1; i--) {
        size_t j = (size_t)bounded(i);
        swap(keys[i - 1], keys[j]);
    }

    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "cannot write %s\n", path);
        return 1;
    }

    fprintf(f, "%lld %lld\n", V, E);
    for (long long k : keys)
        fprintf(f, "%lld %lld\n", k / V, k % V);
    fclose(f);

    fprintf(stderr, "wrote %s  (V=%lld, E=%lld, seed=%u)\n", path, V, E, seed);
    return 0;
}
