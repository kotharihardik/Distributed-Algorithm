#include <mpi.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>

using namespace std;

struct Tally
{
    long long id;
    long long count;
    double resp_sum;
    long long bytes;
};

bool compareTally(const Tally &a, const Tally &b)
{
    if (a.count != b.count) return a.count > b.count;
    return a.id < b.id;
}

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    const char *filename = nullptr;
    bool showTime = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--time") == 0) showTime = true;
        else if (!filename) filename = argv[i];
    }

    if (!filename) {
        if (rank == 0)
            fprintf(stderr, "usage: %s <logfile> [--time]\n", argv[0]);

        MPI_Finalize();
        return 1;
    }

    MPI_Barrier(MPI_COMM_WORLD);
    double start = MPI_Wtime();

    // N, K, S, data start, file size
    long long info[5] = {0, 0, 0, 0, 0};

    if (rank == 0) {
        FILE *f = fopen(filename, "rb");

        if (!f) {
            fprintf(stderr, "cannot open %s\n", filename);
            info[0] = -1;
        } else {
            char line[256];

            if (!fgets(line, sizeof(line), f) ||
                sscanf(line, "%lld %lld %lld",
                       &info[0], &info[1], &info[2]) != 3) {
                fprintf(stderr, "bad header in %s\n", filename);
                info[0] = -1;
            } else {
                info[3] = ftell(f);
                fseek(f, 0, SEEK_END);
                info[4] = ftell(f);
            }

            fclose(f);
        }
    }

    MPI_Bcast(info, 5, MPI_LONG_LONG, 0, MPI_COMM_WORLD);

    if (info[0] < 0) {
        MPI_Finalize();
        return 1;
    }

    long long N = info[0], K = info[1], S = info[2];
    long long dataStart = info[3], fileSize = info[4];

    // Find this process's part of the file
    long long dataSize = fileSize - dataStart;
    long long low = dataStart + dataSize * rank / size;
    long long high = dataStart + dataSize * (rank + 1) / size;

    FILE *f = fopen(filename, "rb");
    if (!f) {
        MPI_Finalize();
        return 1;
    }

    // Move the starting point to a complete line
    if (low > dataStart) {
        fseek(f, low - 1, SEEK_SET);

        int c;
        while ((c = fgetc(f)) != EOF && c != '\n') {}

        low = ftell(f);
    } else {
        fseek(f, low, SEEK_SET);
    }

    vector<char> buf;

    if (high > low) {
        buf.resize((size_t)(high - low));

        size_t got = fread(buf.data(), 1, (size_t)(high - low), f);
        buf.resize(got);

        // Finish the line if we stopped in the middle of one
        if (got > 0 && buf[got - 1] != '\n') {
            int c;
            while ((c = fgetc(f)) != EOF && c != '\n')
                buf.push_back((char)c);
        }
    }

    buf.push_back('\0');
    fclose(f);

    double readTime = MPI_Wtime();

    // Local totals
    long long total = 0, ok = 0, failed = 0;
    long long s2xx = 0, s3xx = 0, s4xx = 0, s5xx = 0;
    long long totalBytes = 0;

    double respSum = 0.0;
    double respMin = numeric_limits<double>::max();
    double respMax = -numeric_limits<double>::max();

    vector<long long> serverCount(S > 0 ? S : 0, 0);
    vector<double> serverResp(S > 0 ? S : 0, 0.0);

    // Store these first because their maximum size is not known yet
    vector<long long> rawEp, rawBytes, rawIv;
    long long localEpMax = -1;
    long long localIvMin = numeric_limits<long long>::max();
    long long localIvMax = numeric_limits<long long>::min();

    rawEp.reserve(4096);
    rawBytes.reserve(4096);
    rawIv.reserve(4096);

    char *p = buf.data();
    char *end = buf.data() + buf.size();

    while (p < end && *p) {
        char *next;

        long long ts = strtoll(p, &next, 10);
        if (next == p) break;
        p = next;

        long long srv = strtoll(p, &next, 10);
        if (next == p) break;
        p = next;

        long long ep = strtoll(p, &next, 10);
        if (next == p) break;
        p = next;

        // user_id is not needed
        strtoll(p, &next, 10);
        if (next == p) break;
        p = next;

        long long status = strtoll(p, &next, 10);
        if (next == p) break;
        p = next;

        double rt = strtod(p, &next);
        if (next == p) break;
        p = next;

        long long bytes = strtoll(p, &next, 10);
        if (next == p) break;
        p = next;

        total++;

        if (status < 400) ok++;
        else failed++;

        if (status >= 200 && status < 300) s2xx++;
        else if (status >= 300 && status < 400) s3xx++;
        else if (status >= 400 && status < 500) s4xx++;
        else if (status >= 500 && status < 600) s5xx++;

        totalBytes += bytes;
        respSum += rt;

        if (rt < respMin) respMin = rt;
        if (rt > respMax) respMax = rt;

        if (srv >= 0 && srv < (long long)serverCount.size()) {
            serverCount[srv]++;
            serverResp[srv] += rt;
        }

        long long interval = ts / 60;

        rawEp.push_back(ep);
        rawBytes.push_back(bytes);
        rawIv.push_back(interval);

        if (ep > localEpMax) localEpMax = ep;
        if (interval < localIvMin) localIvMin = interval;
        if (interval > localIvMax) localIvMax = interval;
    }

    // Find common sizes for endpoint and interval arrays
    long long epMax, ivMin, ivMax;

    MPI_Allreduce(&localEpMax, &epMax, 1,
                   MPI_LONG_LONG, MPI_MAX, MPI_COMM_WORLD);

    MPI_Allreduce(&localIvMin, &ivMin, 1,
                   MPI_LONG_LONG, MPI_MIN, MPI_COMM_WORLD);

    MPI_Allreduce(&localIvMax, &ivMax, 1,
                   MPI_LONG_LONG, MPI_MAX, MPI_COMM_WORLD);

    long long numEp = epMax >= 0 ? epMax + 1 : 0;
    long long numIv = ivMax >= ivMin ? ivMax - ivMin + 1 : 0;

    vector<long long> epCount(numEp, 0);
    vector<long long> epBytes(numEp, 0);
    vector<long long> ivCount(numIv, 0);

    for (size_t i = 0; i < rawEp.size(); i++) {
        if (rawEp[i] >= 0 && rawEp[i] < numEp) {
            epCount[rawEp[i]]++;
            epBytes[rawEp[i]] += rawBytes[i];
        }

        long long pos = rawIv[i] - ivMin;
        if (pos >= 0 && pos < numIv)
            ivCount[pos]++;
    }

    double countTime = MPI_Wtime();

    // Reduce basic values
    long long local[8] =
        {total, ok, failed, s2xx, s3xx, s4xx, s5xx, totalBytes};

    long long global[8] = {0};

    MPI_Reduce(local, global, 8, MPI_LONG_LONG,
               MPI_SUM, 0, MPI_COMM_WORLD);

    // Reduce response times
    double globalRespSum = 0.0;
    double globalRespMin = 0.0;
    double globalRespMax = 0.0;

    MPI_Reduce(&respSum, &globalRespSum, 1,
               MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    MPI_Reduce(&respMin, &globalRespMin, 1,
               MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);

    MPI_Reduce(&respMax, &globalRespMax, 1,
               MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    // Reduce server information
    vector<long long> globalServerCount(rank == 0 ? S : 0, 0);
    vector<double> globalServerResp(rank == 0 ? S : 0, 0.0);

    if (S > 0) {
        MPI_Reduce(serverCount.data(),
                   rank == 0 ? globalServerCount.data() : nullptr,
                   (int)S, MPI_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);

        MPI_Reduce(serverResp.data(),
                   rank == 0 ? globalServerResp.data() : nullptr,
                   (int)S, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    }

    // Reduce endpoint information
    vector<long long> globalEpCount(rank == 0 ? numEp : 0, 0);
    vector<long long> globalEpBytes(rank == 0 ? numEp : 0, 0);

    if (numEp > 0) {
        MPI_Reduce(epCount.data(),
                   rank == 0 ? globalEpCount.data() : nullptr,
                   (int)numEp, MPI_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);

        MPI_Reduce(epBytes.data(),
                   rank == 0 ? globalEpBytes.data() : nullptr,
                   (int)numEp, MPI_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
    }

    // Reduce interval information
    vector<long long> globalIvCount(rank == 0 ? numIv : 0, 0);

    if (numIv > 0) {
        MPI_Reduce(ivCount.data(),
                   rank == 0 ? globalIvCount.data() : nullptr,
                   (int)numIv, MPI_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
    }

    double endTime = MPI_Wtime();

    if (rank == 0) {
        long long busyId = 0, busyCount = 0;

        for (long long i = 0; i < numIv; i++) {
            if (globalIvCount[i] > busyCount) {
                busyCount = globalIvCount[i];
                busyId = ivMin + i;
            }
        }

        // Top servers
        vector<Tally> servers;

        for (long long i = 0; i < S; i++)
            if (globalServerCount[i] > 0)
                servers.push_back(
                    {i, globalServerCount[i], globalServerResp[i], 0});

        sort(servers.begin(), servers.end(), compareTally);

        // Top endpoints
        vector<Tally> endpoints;

        for (long long i = 0; i < numEp; i++)
            if (globalEpCount[i] > 0)
                endpoints.push_back(
                    {i, globalEpCount[i], 0.0, globalEpBytes[i]});

        sort(endpoints.begin(), endpoints.end(), compareTally);

        // Required output
        printf("TOTAL_REQUESTS %lld\n", global[0]);
        printf("SUCCESSFUL_REQUESTS %lld\n", global[1]);
        printf("FAILED_REQUESTS %lld\n", global[2]);

        printf("AVERAGE_RESPONSE_TIME %.6f\n",
               global[0] ? globalRespSum / global[0] : 0.0);

        printf("MIN_RESPONSE_TIME %.6f\n",
               global[0] ? globalRespMin : 0.0);

        printf("MAX_RESPONSE_TIME %.6f\n",
               global[0] ? globalRespMax : 0.0);

        printf("TOTAL_BYTES %lld\n", global[7]);
        printf("STATUS_2XX %lld\n", global[3]);
        printf("STATUS_3XX %lld\n", global[4]);
        printf("STATUS_4XX %lld\n", global[5]);
        printf("STATUS_5XX %lld\n", global[6]);

        printf("BUSIEST_INTERVAL %lld %lld\n", busyId, busyCount);

        printf("TOP_SERVERS\n");

        for (size_t i = 0;
             i < servers.size() && (long long)i < K; i++)
        {
            printf("%lld %lld %.6f\n",
                   servers[i].id,
                   servers[i].count,
                   servers[i].count
                       ? servers[i].resp_sum / servers[i].count
                       : 0.0);
        }

        printf("TOP_ENDPOINTS\n");

        for (size_t i = 0;
             i < endpoints.size() && (long long)i < K; i++)
        {
            printf("%lld %lld %lld\n",
                   endpoints[i].id,
                   endpoints[i].count,
                   endpoints[i].bytes);
        }
    }

    if (showTime) {
        double times[4] = {
            readTime - start,
            countTime - readTime,
            endTime - countTime,
            endTime - start
        };

        double worst[4];

        MPI_Reduce(times, worst, 4, MPI_DOUBLE,
                   MPI_MAX, 0, MPI_COMM_WORLD);

        if (rank == 0) {
            fprintf(stderr, "procs %d  N %lld  K %lld  S %lld\n",
                    size, N, K, S);
            fprintf(stderr, "read     %.6f\n", worst[0]);
            fprintf(stderr, "compute  %.6f\n", worst[1]);
            fprintf(stderr, "reduce   %.6f\n", worst[2]);
            fprintf(stderr, "total    %.6f\n", worst[3]);
        }
    }

    MPI_Finalize();
    return 0;
}