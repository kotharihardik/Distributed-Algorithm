#include <cstdio>
#include <cstdlib>
#include <random>

using namespace std;

int main(int argc, char **argv)
{
    if (argc < 6)
    {
        fprintf(stderr,
                "usage: %s <N> <K> <S> <seed> <outfile> [endpoints]\n"
                "  N          number of log lines\n"
                "  K          size of each top-K list\n"
                "  S          number of servers (ids 0..S-1)\n"
                "  seed       fixes the output; same seed, same file\n"
                "  endpoints  distinct endpoint ids, default 200\n",
                argv[0]);
        return 1;
    }

    long long N = atoll(argv[1]);
    long long K = atoll(argv[2]);
    long long S = atoll(argv[3]);
    unsigned seed = (unsigned)strtoul(argv[4], nullptr, 10);
    const char *path = argv[5];
    long long E = argc > 6 ? atoll(argv[6]) : 200;

    if (N < 1 || K < 1 || S < 1 || E < 1)
    {
        fprintf(stderr, "N, K, S and endpoints must all be positive\n");
        return 1;
    }

    mt19937 rng(seed);

    // Generate a random number from 0 to n-1
    auto bounded = [&rng](unsigned long long n) -> unsigned long long
    {
        unsigned long long bucket = (1ULL << 32) / n;
        unsigned long long cutoff = bucket * n;

        while (true)
        {
            unsigned long long r = rng();

            if (r < cutoff)
                return r / bucket;
        }
    };

    // Some ids get more requests than others
    auto skewed = [&](long long range) -> long long
    {
        long long hot = range / 5;

        if (hot < 1)
            hot = 1;

        if (bounded(100) < 70)
            return (long long)bounded(hot);

        return (long long)bounded(range);
    };

    FILE *f = fopen(path, "w");

    if (!f)
    {
        fprintf(stderr, "cannot write %s\n", path);
        return 1;
    }

    fprintf(f, "%lld %lld %lld\n", N, K, S);

    long long ts = 1700000000;

    for (long long i = 0; i < N; i++)
    {
        ts += (long long)bounded(4);

        long long server = skewed(S);
        long long endpoint = skewed(E);
        long long user = (long long)bounded(100000);

        long long roll = (long long)bounded(100);
        long long status;

        if (roll < 85)
            status = 200 + (long long)bounded(3);
        else if (roll < 90)
            status = 301 + (long long)bounded(2);
        else if (roll < 97)
            status = 400 + (long long)bounded(5);
        else
            status = 500 + (long long)bounded(4);

        double response = (double)(1 + bounded(20000)) / 10.0;
        long long bytes = 100 + (long long)bounded(60000);

        fprintf(f, "%lld %lld %lld %lld %lld %.1f %lld\n",
                ts, server, endpoint, user, status, response, bytes);
    }

    fclose(f);

    fprintf(stderr,
            "wrote %s (N=%lld, K=%lld, S=%lld, endpoints=%lld, seed=%u)\n",
            path, N, K, S, E, seed);

    return 0;
}