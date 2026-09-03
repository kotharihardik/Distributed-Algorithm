#include <iostream>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <chrono>

using namespace std;

int main(int argc, char* argv[])
{
    const char* fileName = NULL;
    bool showTime = false;

    // Read command line arguments
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--time") == 0) showTime = true;

        else if (fileName == NULL) fileName = argv[i];
 
    }

    if (fileName == NULL)
    {
        cout << "Usage: " << argv[0] << " <graph> [--time]\n";
        return 1;
    }

    FILE* file = fopen(fileName, "r");

    if (file == NULL)
    {
        cout << "Cannot open file\n";
        return 1;
    }

    int V, declaredEdges;

    // Read number of vertices and declared number of edges
    if (fscanf(file, "%d %d", &V, &declaredEdges) != 2)
    {
        cout << "Bad input file\n";
        fclose(file);
        return 1;
    }

    vector<int> edgeU;
    vector<int> edgeV;

    edgeU.reserve(declaredEdges);
    edgeV.reserve(declaredEdges);

    int u, v;

    // Read all edges
    while (fscanf(file, "%d %d", &u, &v) == 2)
    {
        // Ignore self loops
        if (u == v)
            continue;

        // Ignore invalid vertices
        if (u < 0 || v < 0 || u >= V || v >= V)
            continue;

        edgeU.push_back(u);
        edgeV.push_back(v);
    }

    fclose(file);

    int E = edgeU.size();

    auto start = chrono::steady_clock::now();

    // Create adjacency list
    vector<vector<int>> adj(V);

    for (int i = 0; i < E; i++)
    {
        adj[edgeU[i]].push_back(edgeV[i]);
        adj[edgeV[i]].push_back(edgeU[i]);
    }

    // Sort neighbours of every vertex
    for (int i = 0; i < V; i++)
    {
        sort(adj[i].begin(), adj[i].end());
    }

    long long count = 0;

    // Check every edge
    for (int i = 0; i < E; i++)
    {
        int u = edgeU[i];
        int v = edgeV[i];

        size_t p = 0;
        size_t q = 0;

        // Find common neighbours of u and v
        while (p < adj[u].size() && q < adj[v].size())
        {
            if (adj[u][p] == adj[v][q])
            {
                count++;
                p++;
                q++;
            }
            else if (adj[u][p] < adj[v][q])  p++;
            else q++;

        }
    }

    // Every triangle is counted once for each of its 3 edges
    long long triangles = count / 3;

    auto end = chrono::steady_clock::now();

    cout << triangles << endl;

    if (showTime)
    {
        double seconds = chrono::duration<double>(end - start).count();

        fprintf(stderr, "procs 1  V %d  E %d\n", V, E);
        fprintf(stderr, "count    %.6f\n", seconds);
        fprintf(stderr, "total    %.6f\n", seconds);
    }

    return 0;
}