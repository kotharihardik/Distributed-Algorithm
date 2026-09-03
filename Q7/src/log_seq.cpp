#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <chrono>
#include <limits>
#define ll long long 

using namespace std;

struct Data{
    ll id;
    ll count;
    double response;
    ll bytes;
};

bool compareData(Data a, Data b){
    if(a.count != b.count)
        return a.count > b.count;

    return a.id < b.id;
}

int main(int argc, char *argv[]){
    if(argc < 2){
        cout << "Usage: " << argv[0] << " <logfile> [--time]" << endl;
        return 1;
    }

    string fileName = argv[1];

    bool showTime = false;

    for(int i = 2; i < argc; i++){
        string arg = argv[i];

        if(arg == "--time")
            showTime = true;
    }

    ifstream fin(fileName.c_str());
    if(!fin){
        cout << "Cannot open file" << endl;
        return 1;
    }

    ll N, K, S;
    fin >> N >> K >> S;

    if(fin.fail()){
        cout << "Invalid input file" << endl;
        return 1;
    }

    chrono::steady_clock::time_point start = chrono::steady_clock::now();

  
    ll totalRequests = 0;
    ll successful = 0;
    ll failed = 0;

    ll status2xx = 0;
    ll status3xx = 0;
    ll status4xx = 0;
    ll status5xx = 0;

    ll totalBytes = 0;

    double responseSum = 0;
    double minResponse = numeric_limits<double>::max();
    double maxResponse = -numeric_limits<double>::max();

    // Information about servers
    vector<ll> serverCount(S, 0);
    vector<double> serverResponse(S, 0);

    // Information about endpoints
    map<ll, ll> endpointCount;
    map<ll, ll> endpointBytes;

    // Information about 60-second intervals
    map<ll, ll> intervalCount;

    ll timestamp;
    ll server;
    ll endpoint;
    ll user;
    ll status;
    ll bytes;
    double responseTime;

    // Read every log line
    while(fin >> timestamp >> server >> endpoint >> user >> status >> responseTime >> bytes){

        totalRequests++;

        // Successful means status code is less than 400
        if(status < 400)
            successful++;
        else
            failed++;

        // Count status codes
        if(status >= 200 && status < 300)
            status2xx++;
        else if(status >= 300 && status < 400)
            status3xx++;
        else if(status >= 400 && status < 500)
            status4xx++;
        else if(status >= 500 && status < 600)
            status5xx++;

        // Response time information
        responseSum += responseTime;

        if(responseTime < minResponse)
            minResponse = responseTime;

        if(responseTime > maxResponse)
            maxResponse = responseTime;

        // Total bytes
        totalBytes += bytes;

        // Server information
        if(server >= 0 && server < S){
            serverCount[server]++;
            serverResponse[server] += responseTime;
        }

        // Endpoint information
        endpointCount[endpoint]++;
        endpointBytes[endpoint] += bytes;

        // Each interval contains 60 seconds
        ll interval = timestamp / 60;
        intervalCount[interval]++;
    }

    fin.close();

    chrono::steady_clock::time_point end = chrono::steady_clock::now();

    // now we Find the busiest interval.
    // The map is ordered by key, so on a tie the smallest interval id wins.
    ll busiestInterval = 0;
    ll busiestCount = 0;

    for(auto it : intervalCount){
        if(it.second > busiestCount){
            busiestCount = it.second;
            busiestInterval = it.first;
        }
    }

    // Create list of servers
    vector<Data> servers;

    for(ll i = 0; i < S; i++){
        if(serverCount[i] > 0){
            Data temp;

            temp.id = i;
            temp.count = serverCount[i];
            temp.response = serverResponse[i];
            temp.bytes = 0;

            servers.push_back(temp);
        }
    }

    // Sorting  servers based on comparision
    sort(servers.begin(), servers.end(), compareData);

    // Create list of endpoints
    vector<Data> endpoints;

    for(auto it : endpointCount){
        Data temp;

        temp.id = it.first;
        temp.count = it.second;
        temp.response = 0;
        temp.bytes = endpointBytes[it.first];

        endpoints.push_back(temp);
    }

    // Sort endpoints
    sort(endpoints.begin(), endpoints.end(), compareData);

    cout << fixed << setprecision(6);

    cout << "TOTAL_REQUESTS " << totalRequests << endl;
    cout << "SUCCESSFUL_REQUESTS " << successful << endl;
    cout << "FAILED_REQUESTS " << failed << endl;

    if(totalRequests > 0)
        cout << "AVERAGE_RESPONSE_TIME " << responseSum / totalRequests << endl;
    else
        cout << "AVERAGE_RESPONSE_TIME 0.000000" << endl;

    if(totalRequests > 0)
        cout << "MIN_RESPONSE_TIME " << minResponse << endl;
    else
        cout << "MIN_RESPONSE_TIME 0.000000" << endl;

    if(totalRequests > 0)
        cout << "MAX_RESPONSE_TIME " << maxResponse << endl;
    else
        cout << "MAX_RESPONSE_TIME 0.000000" << endl;

    cout << "TOTAL_BYTES " << totalBytes << endl;

    cout << "STATUS_2XX " << status2xx << endl;
    cout << "STATUS_3XX " << status3xx << endl;
    cout << "STATUS_4XX " << status4xx << endl;
    cout << "STATUS_5XX " << status5xx << endl;

    cout << "BUSIEST_INTERVAL "
         << busiestInterval << " "
         << busiestCount << endl;

    // Print top servers
    cout << "TOP_SERVERS" << endl;

    for(ll i = 0; i < K && i < (ll)servers.size(); i++){

        double averageResponse = servers[i].response / servers[i].count;
        cout << servers[i].id << " " << servers[i].count << " "<< averageResponse << endl;
    }

    // Print top endpoints
    cout << "TOP_ENDPOINTS" << endl;

    for(ll i = 0; i < K && i < (ll)endpoints.size(); i++){
        cout << endpoints[i].id << " "<< endpoints[i].count << " "<< endpoints[i].bytes << endl;
    }

    if(showTime){
        double seconds = chrono::duration<double>(end - start).count();

        cerr.setf(ios::fixed);
        cerr.precision(6);
        cerr << "procs 1  N " << N << "  K " << K << "  S " << S << endl;
        cerr << "compute " << seconds << endl;
        cerr << "total " << seconds << endl;
    }

    return 0;
}
