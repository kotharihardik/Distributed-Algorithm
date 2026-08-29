#include <mpi.h>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace std;


/* Read graph */
bool read_graph(const char *fileName, int &V,
                vector<int> &eu, vector<int> &ev)
{
    FILE *f = fopen(fileName, "rb");
    if (!f) {
        fprintf(stderr, "cannot open %s\n", fileName);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    vector<char> data(size + 1);
    size_t n = fread(data.data(), 1, size, f);
    data[n] = '\0';
    fclose(f);

    const char *p =data.data();
    const char *end = data.data() + n;

    auto next_int = [&](int &x) {
        while (p < end && (*p < '0' || *p > '9')) p++;
        if (p >= end) return false;

        x = 0;
        while (p < end && *p >= '0' && *p <= '9') {
            x = x *10 + (*p - '0');
            p++;
        }
        return true;
    };

    int E;

    if (!next_int(V) || !next_int(E) || V <= 0) {
        fprintf(stderr, "bad input\n");
        return false;
    }

    eu.reserve(E);
    ev.reserve(E);

    int u,v;

    while (next_int(u)) {
        if (!next_int(v)) break;
        if (u == v) continue;
        if (u < 0 || v < 0 || u >= V || v>= V) continue;

        eu.push_back(u);
        ev.push_back(v);
    }

    return true;
}


/* Build forward adjacency */
void build_forward(int V, const vector<int> &eu, const vector<int> &ev,
                   vector<int> &start, vector<int> &adj,
                   vector<int> &outU, vector<int> &outV)
{
    int E = eu.size();

    vector<int> degree(V, 0);

    for (int i = 0; i<E; i++) {
        degree[eu[i]]++;
        degree[ev[i]]++;
    }

    /* Orient every edge using degree, then vertex id */
    outU.resize(E);
    outV.resize(E);

    for (int i = 0; i < E; i++) {
        int u = eu[i], v =ev[i];

        if (degree[u] < degree[v] ||
            (degree[u] == degree[v] && u < v)) {
            outU[i] = u;
            outV[i] = v;
        }
        else {
            outU[i] = v;
            outV[i] = u;
        }
    }

    /* Count edges by their head */
    vector<int> head(V + 1, 0);

    for (int i = 0; i < E; i++)
        head[outV[i]]++;

    for (int i = 0; i < V; i++)
        head[i + 1] += head[i];

    vector<int> order(E);

    for (int i = E - 1; i >= 0; i--) {
        int v = outV[i];
        head[v]--;
        order[head[v]] = i;
    }

    /* Find starting position of every forward list */
    start.assign(V + 1, 0);

    for (int i = 0; i < E; i++)
        start[outU[i]]++;

    int pos = 0;

    for (int i = 0; i < V; i++) {
        int c = start[i];
        start[i] = pos;
        pos += c;
    }

    start[V] = pos;

    vector<int> next = start;
    adj.resize(E);

    for (int i = 0; i < E; i++) {
        int e = order[i];
        adj[next[outU[e]]++] = outV[e];
    }
}


/* Count common neighbours */
long long common(const int *a, int n, const int *b, int m)
{
    int i = 0, j = 0;
    long long count = 0;

    while (i < n && j < m) {
        if (a[i] == b[j]) {
            count++;
            i++;
            j++;
        }
        else if (a[i] < b[j]) {
            i++;
        }
        else {
            j++;
        }
    }

    return count;
}


int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    const char *fileName = nullptr;
    bool showTime = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--time") == 0)
            showTime = true;
        else if (!fileName)
            fileName = argv[i];
    }

    if (!fileName) {
        if (rank == 0)
            fprintf(stderr, "usage: %s <graph> [--time]\n", argv[0]);

        MPI_Finalize();
        return 1;
    }

    int V = 0;
    vector<int> eu, ev;

    double t0 = MPI_Wtime();

    if (rank == 0 && !read_graph(fileName, V, eu, ev))
        V = -1;

    int info[2] = {V, rank == 0 ? (int)eu.size() : 0};

    MPI_Bcast(info, 2, MPI_INT, 0, MPI_COMM_WORLD);

    V = info[0];
    int E = info[1];

    if (V < 0) {
        MPI_Finalize();
        return 1;
    }

    double t1 = MPI_Wtime();

    if (rank != 0) {
        eu.resize(E);
        ev.resize(E);
    }

    if (E > 0) {
        MPI_Bcast(eu.data(), E, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(ev.data(), E, MPI_INT, 0, MPI_COMM_WORLD);
    }

    double t2 = MPI_Wtime();

    vector<int> start, adj, outU, outV;

    if (E > 0)
        build_forward(V, eu, ev, start, adj, outU, outV);

    double t3 = MPI_Wtime();

    /* Divide edges between processes */
    int base = E / size;
    int extra = E % size;

    int lo = rank * base + (rank < extra ? rank : extra);
    int hi = lo + base + (rank < extra ? 1 : 0);

    long long local = 0;

    for (int i = lo; i < hi; i++) {
        int u = outU[i];
        int v = outV[i];

        local += common(&adj[start[u]], start[u + 1] - start[u],
                        &adj[start[v]], start[v + 1] - start[v]);
    }

    double t4 = MPI_Wtime();

    long long total = 0;

    MPI_Reduce(&local, &total, 1, MPI_LONG_LONG,
               MPI_SUM, 0, MPI_COMM_WORLD);

    double t5 = MPI_Wtime();

    if (rank == 0)
        printf("%lld\n", total);

    if (showTime) {
        double times[6] = {
            t1 - t0,
            t2 - t1,
            t3 - t2,
            t4 - t3,
            t5 - t4,
            t5 - t0
        };

        double maxTimes[6];

        MPI_Reduce(times, maxTimes, 6, MPI_DOUBLE,
                   MPI_MAX, 0, MPI_COMM_WORLD);

        if (rank == 0) {
            fprintf(stderr, "procs %d  V %d  E %d\n", size, V, E);
            fprintf(stderr, "read     %.6f\n", maxTimes[0]);
            fprintf(stderr, "bcast    %.6f\n", maxTimes[1]);
            fprintf(stderr, "build    %.6f\n", maxTimes[2]);
            fprintf(stderr, "count    %.6f\n", maxTimes[3]);
            fprintf(stderr, "reduce   %.6f\n", maxTimes[4]);
            fprintf(stderr, "total    %.6f\n", maxTimes[5]);
        }
    }

    MPI_Finalize();
    return 0;
}