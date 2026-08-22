/*
 * Q4 - Counting triangles in an undirected graph with MPI.
 *
 * Method: orient every edge, then intersect forward adjacency lists.
 *
 * Give each vertex a rank, ordered by degree and broken by id:
 *
 *      u before v   <=>   (deg[u], u) < (deg[v], v)
 *
 * and point every edge from the earlier vertex to the later one. Write N+(x)
 * for the vertices x points at. For a triangle {a, b, c} with a before b
 * before c, the edges come out a->b, a->c, b->c, and c turns up in N+(a) and
 * in N+(b) but nowhere else. So walking the edges and adding |N+(u) & N+(v)|
 * for each one finds every triangle exactly once - no halving, no divide by
 * three, and nothing to undo afterwards.
 *
 * That last property is what makes the parallel version easy: the edges are
 * split into equal blocks, one block per process, and because a triangle is
 * only ever counted at one particular edge, two processes can never count the
 * same triangle. A single MPI_Reduce at the end adds the blocks up.
 *
 * Ordering by degree rather than by id matters. With plain id order a
 * low-numbered hub ends up with almost all of its edges pointing outward and
 * its forward list becomes enormous; by degree, no forward list is longer
 * than about sqrt(2E).
 *
 * Build : mpicxx -O2 -std=c++17 -o tri_mpi tri_mpi.cpp
 * Run   : mpirun -np 4 ./tri_mpi graph.txt
 */

#include <mpi.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using namespace std;

/* ---------- input ---------------------------------------------------- */

/*
 * Up to a million edges means two million numbers to parse, and scanf spends
 * more time on that than the triangle count itself takes. Slurp the file and
 * pick the integers out by hand instead.
 */
static bool read_graph(const char *path, int &V, vector<int> &eu, vector<int> &ev)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "cannot open %s\n", path);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    vector<char> buf(size + 1);
    size_t got = fread(buf.data(), 1, size, f);
    buf[got] = '\0';
    fclose(f);

    const char *p = buf.data();
    const char *end = buf.data() + got;

    /* pulls the next non-negative integer out of the buffer */
    auto next_int = [&](int &out) -> bool {
        while (p < end && (*p < '0' || *p > '9')) p++;
        if (p >= end) return false;
        int val = 0;
        while (p < end && *p >= '0' && *p <= '9') {
            val = val * 10 + (*p - '0');
            p++;
        }
        out = val;
        return true;
    };

    int declared_edges = 0;
    if (!next_int(V) || !next_int(declared_edges)) {
        fprintf(stderr, "bad header in %s (expected: V E)\n", path);
        return false;
    }
    if (V <= 0) {
        fprintf(stderr, "V must be positive\n");
        return false;
    }

    eu.clear();
    ev.clear();
    eu.reserve(declared_edges);
    ev.reserve(declared_edges);

    int u, v;
    while (next_int(u)) {
        if (!next_int(v)) break;
        if (u == v) continue;                       /* self-loop, no triangle */
        if (u < 0 || v < 0 || u >= V || v >= V) continue;
        eu.push_back(u);
        ev.push_back(v);
    }

    return true;
}

/* ---------- shared by both the counting and the setup ----------------- */

/*
 * Builds the forward adjacency in compressed form: fadj holds every forward
 * neighbour, and fstart[x] .. fstart[x+1] delimits the block belonging to x.
 *
 * Each block comes out in increasing order, which the intersection below
 * relies on. That is arranged without any comparison sort: bucket the edges
 * by their head first, then walk that ordering while filling the blocks by
 * tail. Because the second pass is stable, the heads inside every block are
 * already ascending. Two linear passes instead of E log E.
 */
static void build_forward(int V, const vector<int> &eu, const vector<int> &ev,
                          vector<int> &fstart, vector<int> &fadj,
                          vector<int> &oa, vector<int> &ob)
{
    size_t m = eu.size();

    vector<int> deg(V, 0);
    for (size_t i = 0; i < m; i++) {
        deg[eu[i]]++;
        deg[ev[i]]++;
    }

    /* orient: low degree first, id settles ties */
    oa.resize(m);
    ob.resize(m);
    for (size_t i = 0; i < m; i++) {
        int u = eu[i], v = ev[i];
        bool u_first = (deg[u] < deg[v]) || (deg[u] == deg[v] && u < v);
        oa[i] = u_first ? u : v;
        ob[i] = u_first ? v : u;
    }

    /* pass 1 - order the edges by head */
    vector<int> head_count(V + 1, 0);
    for (size_t i = 0; i < m; i++) head_count[ob[i]]++;
    for (int x = 0; x < V; x++) head_count[x + 1] += head_count[x];

    vector<int> by_head(m);
    for (size_t i = m; i-- > 0; ) by_head[--head_count[ob[i]]] = (int)i;

    /* pass 2 - fill the blocks by tail, keeping pass 1's order */
    fstart.assign(V + 1, 0);
    for (size_t i = 0; i < m; i++) fstart[oa[i]]++;
    int running = 0;
    for (int x = 0; x < V; x++) {
        int c = fstart[x];
        fstart[x] = running;
        running += c;
    }
    fstart[V] = running;

    vector<int> fill = fstart;
    fadj.resize(m);
    for (size_t k = 0; k < m; k++) {
        int e = by_head[k];
        fadj[fill[oa[e]]++] = ob[e];
    }
}

/* how many vertices appear in both blocks - ordinary two-pointer walk */
static inline long long shared(const int *a, int alen, const int *b, int blen)
{
    long long hits = 0;
    int i = 0, j = 0;
    while (i < alen && j < blen) {
        if (a[i] == b[j]) { hits++; i++; j++; }
        else if (a[i] < b[j]) i++;
        else j++;
    }
    return hits;
}

/* ---------- main ------------------------------------------------------ */

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    const char *in_path = nullptr;
    bool show_time = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--time") == 0) show_time = true;
        else if (!in_path) in_path = argv[i];
    }

    if (!in_path) {
        if (rank == 0) fprintf(stderr, "usage: %s <graph> [--time]\n", argv[0]);
        MPI_Finalize();
        return 1;
    }

    int V = 0;
    vector<int> eu, ev;

    double t_start = MPI_Wtime();

    if (rank == 0 && !read_graph(in_path, V, eu, ev)) V = -1;

    int header[2] = { V, rank == 0 ? (int)eu.size() : 0 };
    MPI_Bcast(header, 2, MPI_INT, 0, MPI_COMM_WORLD);
    V = header[0];
    int m = header[1];

    if (V < 0) {
        MPI_Finalize();
        return 1;
    }

    double t_read = MPI_Wtime();

    /*
     * Every process needs the whole edge list, because a triangle sitting on
     * one process's edge can close through a vertex owned by any other. At
     * the stated limit of a million edges that is 8 MB per process, which is
     * nothing, and it buys a counting phase with no communication in it at
     * all. The work - the edges each process is responsible for - is what
     * gets divided, and that division is what keeps the count honest.
     */
    if (rank != 0) {
        eu.resize(m);
        ev.resize(m);
    }
    if (m > 0) {
        MPI_Bcast(eu.data(), m, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(ev.data(), m, MPI_INT, 0, MPI_COMM_WORLD);
    }

    double t_bcast = MPI_Wtime();

    vector<int> fstart, fadj, oa, ob;
    if (m > 0) build_forward(V, eu, ev, fstart, fadj, oa, ob);

    double t_build = MPI_Wtime();

    /* this process takes edges [lo, hi); the first m % size blocks get one extra */
    int base = m / size;
    int rem  = m % size;
    int lo = rank * base + (rank < rem ? rank : rem);
    int hi = lo + base + (rank < rem ? 1 : 0);

    long long local = 0;
    for (int i = lo; i < hi; i++) {
        int a = oa[i], b = ob[i];
        local += shared(&fadj[fstart[a]], fstart[a + 1] - fstart[a],
                        &fadj[fstart[b]], fstart[b + 1] - fstart[b]);
    }

    double t_count = MPI_Wtime();

    long long total = 0;
    MPI_Reduce(&local, &total, 1, MPI_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);

    double t_end = MPI_Wtime();

    if (rank == 0) printf("%lld\n", total);

    if (show_time) {
        double mine[6] = { t_read  - t_start,
                           t_bcast - t_read,
                           t_build - t_bcast,
                           t_count - t_build,
                           t_end   - t_count,
                           t_end   - t_start };
        double worst[6];
        MPI_Reduce(mine, worst, 6, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

        if (rank == 0) {
            fprintf(stderr, "procs %d  V %d  E %d\n", size, V, m);
            fprintf(stderr, "read     %.6f\n", worst[0]);
            fprintf(stderr, "bcast    %.6f\n", worst[1]);
            fprintf(stderr, "build    %.6f\n", worst[2]);
            fprintf(stderr, "count    %.6f\n", worst[3]);
            fprintf(stderr, "reduce   %.6f\n", worst[4]);
            fprintf(stderr, "total    %.6f\n", worst[5]);
        }
    }

    MPI_Finalize();
    return 0;
}
