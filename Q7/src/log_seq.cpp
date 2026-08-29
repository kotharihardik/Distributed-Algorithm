#include <iomanip>
#include <iostream>
#include <cstdio>
#include <cstring>
#include <vector>
#include <map>
#include <algorithm>
#include <chrono>
#include <cfloat>

using namespace std;

struct Data
{
    long long id;
    long long count;
    double response;
    long long bytes;
};

bool compareData(Data a, Data b)
{
    if (a.count != b.count)
        return a.count > b.count;

    return a.id < b.id;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        cout << "Usage: " << argv[0] << " <logfile> [--time]" << endl;
        return 1;
    }

    char *filename = argv[1];
    bool showTime = false;

    for (int i = 2; i < argc; i++)
    {
        if (strcmp(argv[i], "--time") == 0)
            showTime = true;
    }

    FILE *file = fopen(filename, "r");

    if (file == NULL)
    {
        cout << "Cannot open file" << endl;
        return 1;
    }

    long long N, K, S;

    if (fscanf(file, "%lld %lld %lld", &N, &K, &S) != 3)
    {
        cout << "Invalid input file" << endl;
        fclose(file);
        return 1;
    }

    auto start = chrono::steady_clock::now();

    long long totalRequests = 0, successful = 0, failed = 0;
    long long status2xx = 0, status3xx = 0, status4xx = 0, status5xx = 0;
    long long totalBytes = 0;

    double responseSum = 0;
    double minResponse = DBL_MAX;
    double maxResponse = -DBL_MAX;

    vector<long long> serverCount(S, 0);
    vector<double> serverResponse(S, 0);

    map<long long, long long> endpointCount;
    map<long long, long long> endpointBytes;
    map<long long, long long> intervalCount;

    long long timestamp, server, endpoint, user, status, bytes;
    double responseTime;

    while (fscanf(file, "%lld %lld %lld %lld %lld %lf %lld",
                  &timestamp, &server, &endpoint, &user,
                  &status, &responseTime, &bytes) == 7)
    {
        totalRequests++;

        if (status < 400)
            successful++;
        else
            failed++;

        if (status >= 200 && status < 300)
            status2xx++;
        else if (status >= 300 && status < 400)
            status3xx++;
        else if (status >= 400 && status < 500)
            status4xx++;
        else if (status >= 500 && status < 600)
            status5xx++;

        totalBytes += bytes;
        responseSum += responseTime;

        if (responseTime < minResponse)
            minResponse = responseTime;

        if (responseTime > maxResponse)
            maxResponse = responseTime;

        if (server >= 0 && server < S)
        {
            serverCount[server]++;
            serverResponse[server] += responseTime;
        }

        endpointCount[endpoint]++;
        endpointBytes[endpoint] += bytes;

        long long interval = timestamp / 60;
        intervalCount[interval]++;
    }

    fclose(file);

    auto end = chrono::steady_clock::now();

    long long busiestInterval = 0, busiestCount = 0;

    for (auto item : intervalCount)
    {
        if (item.second > busiestCount)
        {
            busiestCount = item.second;
            busiestInterval = item.first;
        }
    }

    vector<Data> servers;

    for (long long i = 0; i < S; i++)
    {
        if (serverCount[i] > 0)
            servers.push_back({i, serverCount[i], serverResponse[i], 0});
    }

    sort(servers.begin(), servers.end(), compareData);

    vector<Data> endpoints;

    for (auto item : endpointCount)
    {
        endpoints.push_back(
            {item.first, item.second, 0, endpointBytes[item.first]});
    }

    sort(endpoints.begin(), endpoints.end(), compareData);

    cout << fixed << setprecision(6);

    cout << "TOTAL_REQUESTS " << totalRequests << endl;
    cout << "SUCCESSFUL_REQUESTS " << successful << endl;
    cout << "FAILED_REQUESTS " << failed << endl;

    cout << "AVERAGE_RESPONSE_TIME "
         << (totalRequests ? responseSum / totalRequests : 0.0) << endl;

    cout << "MIN_RESPONSE_TIME "
         << (totalRequests ? minResponse : 0.0) << endl;

    cout << "MAX_RESPONSE_TIME "
         << (totalRequests ? maxResponse : 0.0) << endl;

    cout << "TOTAL_BYTES " << totalBytes << endl;

    cout << "STATUS_2XX " << status2xx << endl;
    cout << "STATUS_3XX " << status3xx << endl;
    cout << "STATUS_4XX " << status4xx << endl;
    cout << "STATUS_5XX " << status5xx << endl;

    cout << "BUSIEST_INTERVAL "
         << busiestInterval << " " << busiestCount << endl;

    cout << "TOP_SERVERS" << endl;

    for (long long i = 0; i < K && i < (long long)servers.size(); i++)
    {
        double averageResponse =
            servers[i].response / servers[i].count;

        printf("%lld %lld %.6f\n",
               servers[i].id, servers[i].count, averageResponse);
    }

    cout << "TOP_ENDPOINTS" << endl;

    for (long long i = 0; i < K && i < (long long)endpoints.size(); i++)
    {
        cout << endpoints[i].id << " "
             << endpoints[i].count << " "
             << endpoints[i].bytes << endl;
    }

    if (showTime)
    {
        double seconds =
            chrono::duration<double>(end - start).count();

        fprintf(stderr, "procs 1  N %lld  K %lld  S %lld\n",
                N, K, S);
        fprintf(stderr, "compute  %.6f\n", seconds);
        fprintf(stderr, "total    %.6f\n", seconds);
    }

    return 0;
}