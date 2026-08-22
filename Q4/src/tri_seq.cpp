/*
 * Q4 - sequential triangle count, used as the reference answer.
 *
 * This deliberately does NOT mirror the MPI version. That one orients the
 * edges by degree and counts each triangle exactly once; if the orientation
 * rule were subtly wrong, a copy of it would agree with itself and the bug
 * would sail straight through. So this one takes the plain undirected route:
 * keep both directions of every edge, and for each edge count the neighbours
 * u and v have in common. Every triangle then turns up three times - once at
 * each of its edges - so the total gets divided by three at the end.
 *
 * Slower, but it shares no logic with the parallel version, which is the
 * whole point of a reference.
 *
 * Build : g++ -O2 -std=c++17 -o tri_seq tri_seq.cpp
 * Run   : ./tri_seq graph.txt
 */

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace std;

int main(int argc, char **argv)
{
    const char *in_path = nullptr;
    bool show_time = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--time") == 0) show_time = true;
        else if (!in_path) in_path = argv[i];
    }

    if (!in_path) {
        fprintf(stderr, "usage: %s <graph> [--time]\n", argv[0]);
        return 1;
    }

    FILE *f = fopen(in_path, "r");
    if (!f) {
        fprintf(stderr, "cannot open %s\n", in_path);
        return 1;
    }

    int V, declared;
    if (fscanf(f, "%d %d", &V, &declared) != 2 || V <= 0) {
        fprintf(stderr, "bad header in %s\n", in_path);
        fclose(f);
        return 1;
    }

    vector<int> eu, ev;
    eu.reserve(declared);
    ev.reserve(declared);

    int u, v;
    while (fscanf(f, "%d %d", &u, &v) == 2) {
        if (u == v) continue;
        if (u < 0 || v < 0 || u >= V || v >= V) continue;
        eu.push_back(u);
        ev.push_back(v);
    }
    fclose(f);

    size_t m = eu.size();

    auto t0 = chrono::steady_clock::now();

    /* adjacency in both directions, each list sorted */
    vector<int> start(V + 1, 0);
    for (size_t i = 0; i < m; i++) {
        start[eu[i]]++;
        start[ev[i]]++;
    }
    int running = 0;
    for (int x = 0; x < V; x++) {
        int c = start[x];
        start[x] = running;
        running += c;
    }
    start[V] = running;

    vector<int> fill_at = start;
    vector<int> adj(running);
    for (size_t i = 0; i < m; i++) {
        adj[fill_at[eu[i]]++] = ev[i];
        adj[fill_at[ev[i]]++] = eu[i];
    }
    for (int x = 0; x < V; x++)
        sort(adj.begin() + start[x], adj.begin() + start[x + 1]);

    /* for every edge, how many neighbours do its endpoints share */
    long long triples = 0;
    for (size_t i = 0; i < m; i++) {
        int a = eu[i], b = ev[i];
        int p = start[a], pe = start[a + 1];
        int q = start[b], qe = start[b + 1];
        while (p < pe && q < qe) {
            if (adj[p] == adj[q]) { triples++; p++; q++; }
            else if (adj[p] < adj[q]) p++;
            else q++;
        }
    }

    long long triangles = triples / 3;   /* each one seen at all three edges */

    auto t1 = chrono::steady_clock::now();

    printf("%lld\n", triangles);

    if (show_time) {
        double secs = chrono::duration<double>(t1 - t0).count();
        fprintf(stderr, "procs 1  V %d  E %zu\n", V, m);
        fprintf(stderr, "count    %.6f\n", secs);
        fprintf(stderr, "total    %.6f\n", secs);
    }

    return 0;
}
