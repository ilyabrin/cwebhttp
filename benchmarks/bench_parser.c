// Parser benchmark - measure throughput (MB/s) for HTTP parsing
// Compares cwebhttp against theoretical maximum (memcpy baseline)

#include "cwebhttp.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
static double GET_TIME_MS()
{
    static LARGE_INTEGER frequency;
    static int initialized = 0;

    if (!initialized)
    {
        QueryPerformanceFrequency(&frequency);
        initialized = 1;
    }

    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (double)(counter.QuadPart * 1000.0) / frequency.QuadPart;
}
#else
#include <sys/time.h>
static double GET_TIME_MS()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000.0) + (tv.tv_usec / 1000.0);
}
#endif

// Test data - realistic HTTP request
static const char *TEST_REQUEST =
    "GET /api/v1/users/12345?fields=name,email&sort=asc HTTP/1.1\r\n"
    "Host: api.example.com\r\n"
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) Chrome/91.0\r\n"
    "Accept: application/json, text/plain, */*\r\n"
    "Accept-Language: en-US,en;q=0.9\r\n"
    "Accept-Encoding: gzip, deflate, br\r\n"
    "Authorization: Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9\r\n"
    "Cookie: session_id=abc123; user_pref=dark_mode\r\n"
    "Connection: keep-alive\r\n"
    "Cache-Control: no-cache\r\n"
    "\r\n";

// Baseline - memcpy throughput (theoretical maximum)
static double bench_memcpy(const char *data, size_t len, int iterations)
{
    char *buf = malloc(len);
    if (!buf)
        return 0.0;

    double start = GET_TIME_MS();

    volatile char dummy = 0;
    for (int i = 0; i < iterations; i++)
    {
        memcpy(buf, data, len);
        dummy += buf[0]; // Prevent optimization
    }

    double elapsed = GET_TIME_MS() - start;
    free(buf);

    double mb_processed = (len * iterations) / (1024.0 * 1024.0);
    return (elapsed > 0) ? mb_processed / (elapsed / 1000.0) : 0.0; // MB/s
}

// Benchmark cwebhttp parser
static double bench_cwebhttp(const char *data, size_t len, int iterations)
{
    // Make a mutable copy for zero-alloc parsing
    char *buf = malloc(len + 1);
    if (!buf)
        return 0.0;

    double start = GET_TIME_MS();

    for (int i = 0; i < iterations; i++)
    {
        memcpy(buf, data, len);
        buf[len] = '\0';

        cwh_request_t req = {0};
        cwh_parse_req(buf, len, &req);
    }

    double elapsed = GET_TIME_MS() - start;
    free(buf);

    double mb_processed = (len * iterations) / (1024.0 * 1024.0);
    return (elapsed > 0) ? mb_processed / (elapsed / 1000.0) : 0.0; // MB/s
}

// Benchmark response parsing
static double bench_cwebhttp_response(int iterations)
{
    const char *response =
        "HTTP/1.1 200 OK\r\n"
        "Date: Mon, 27 Jul 2009 12:28:53 GMT\r\n"
        "Server: Apache/2.2.14 (Win32)\r\n"
        "Last-Modified: Wed, 22 Jul 2009 19:15:56 GMT\r\n"
        "Content-Length: 88\r\n"
        "Content-Type: text/html\r\n"
        "Connection: keep-alive\r\n"
        "\r\n"
        "<html><body><h1>It works!</h1><p>This is a simple HTML response body.</p></body></html>";

    size_t len = strlen(response);
    char *buf = malloc(len + 1);
    if (!buf)
        return 0.0;

    double start = GET_TIME_MS();

    for (int i = 0; i < iterations; i++)
    {
        memcpy(buf, response, len);
        buf[len] = '\0';

        cwh_response_t res = {0};
        cwh_parse_res(buf, len, &res);
    }

    double elapsed = GET_TIME_MS() - start;
    free(buf);

    double mb_processed = (len * iterations) / (1024.0 * 1024.0);
    return (elapsed > 0) ? mb_processed / (elapsed / 1000.0) : 0.0; // MB/s
}

int main(void)
{
    printf("=== cwebhttp Parser Benchmark ===\n\n");

    size_t req_len = strlen(TEST_REQUEST);
    printf("Test request size: %llu bytes\n", (unsigned long long)req_len);
    printf("Running benchmarks (this may take a few seconds)...\n\n");

    // Run multiple iterations for accuracy
    int iterations = 100000;

    // Baseline: memcpy throughput
    double memcpy_speed = bench_memcpy(TEST_REQUEST, req_len, iterations);
    printf("Baseline (memcpy):          %.2f MB/s\n", memcpy_speed);

    // cwebhttp request parser
    double request_speed = bench_cwebhttp(TEST_REQUEST, req_len, iterations);
    printf("cwebhttp (request parser):  %.2f MB/s (%.1f%% of memcpy)\n",
           request_speed, (request_speed / memcpy_speed) * 100.0);

    // cwebhttp response parser
    double response_speed = bench_cwebhttp_response(iterations);
    printf("cwebhttp (response parser): %.2f MB/s (%.1f%% of memcpy)\n",
           response_speed, (response_speed / memcpy_speed) * 100.0);

    printf("\n");
    double total_time_sec = (response_speed > 0) ? ((req_len * iterations) / (1024.0 * 1024.0)) / request_speed : 0;
    double req_per_sec = (total_time_sec > 0) ? iterations / total_time_sec : 0;
    printf("Requests parsed per second: %.0f req/s\n", req_per_sec);

    printf("\nParser efficiency: %.1f%% of theoretical maximum\n",
           ((request_speed + response_speed) / 2.0 / memcpy_speed) * 100.0);

    return 0;
}
