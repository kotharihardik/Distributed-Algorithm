#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>

using namespace std;

int main(int argc, char **argv)
{
    if (argc < 5) {
        fprintf(stderr, "usage: %s <V> <E> <seed> <outfile>\n", argv[0]);
        return 1;
    }

    long long V = atoll(argv[1]);
    long long E = atoll(argv[2]);
    unsigned seed = (unsigned)strtoul(argv[3], nullptr, 10);
    const char *fileName = argv[4];

    if (V < 3 || E < 1) {
        fprintf(stderr, "need V >= 3 and E >= 1\n");
        return 1;
    }

    long long maxEdges = V * (V - 1) / 2;

    if (E > maxEdges) {
        fprintf(stderr, "too many edges\n");
        return 1;
    }

    mt19937 rng(seed);

    /* Random number from 0 to n-1 */
    auto bounded = [&rng](unsigned long long n) {
        unsigned long long bucket = (1ULL << 32) / n;
        unsigned long long limit = bucket * n;

        while (true) {
            unsigned long long r = rng();

            if (r < limit)
                return r / bucket;
        }
    };


    /* Store an edge as one number */
    vector<long long> edges;
    edges.reserve((size_t)(E * 1.15) + 16);


    /* Generate random edges */
    auto draw = [&](long long count) {
        for (long long i = 0; i < count; i++) {
            long long u = bounded(V);
            long long v = bounded(V);

            if (u == v)
                continue;

            if (u > v)
                swap(u, v);

            edges.push_back(u * V + v);
        }
    };


    /*
     * For dense graphs it is faster to create all possible edges
     * and then choose E of them.
     */
    bool dense = (maxEdges <= 50000000LL &&
                  E * 3 > maxEdges);

    if (dense) {
        edges.clear();
        edges.reserve(maxEdges);

        for (long long u = 0; u < V; u++) {
            for (long long v = u + 1; v < V; v++) {
                edges.push_back(u * V + v);
            }
        }

        /* Choose E random edges */
        for (long long i = 0; i < E; i++) {
            long long j = i + bounded(maxEdges - i);
            swap(edges[i], edges[j]);
        }

        edges.resize(E);
    }
    else {
        /*
         * For sparse graphs, generating all possible edges would
         * use too much memory, so generate random edges instead.
         */
        draw((long long)(E * 1.15) + 16);

        sort(edges.begin(), edges.end());
        edges.erase(unique(edges.begin(), edges.end()), edges.end());

        /* Generate more if duplicates made us short */
        int rounds = 0;

        while ((long long)edges.size() < E && rounds < 200) {
            long long missing = E - edges.size();

            draw(missing * 2 + 64);

            sort(edges.begin(), edges.end());
            edges.erase(unique(edges.begin(), edges.end()), edges.end());

            rounds++;
        }

        if ((long long)edges.size() < E) {
            fprintf(stderr, "could not generate enough edges\n");
            return 1;
        }

        edges.resize(E);
    }


    /*
     * Shuffle the edges so the output file does not look sorted.
     */
    for (size_t i = edges.size(); i > 1; i--) {
        size_t j = bounded(i);
        swap(edges[i - 1], edges[j]);
    }


    /* Write graph to file */
    FILE *file = fopen(fileName, "w");

    if (file == NULL) {
        fprintf(stderr, "cannot write %s\n", fileName);
        return 1;
    }

    fprintf(file, "%lld %lld\n", V, E);

    for (long long edge : edges) {
        long long u = edge / V;
        long long v = edge % V;

        fprintf(file, "%lld %lld\n", u, v);
    }

    fclose(file);

    fprintf(stderr, "wrote %s  (V=%lld, E=%lld, seed=%u)\n",
            fileName, V, E, seed);

    return 0;
}