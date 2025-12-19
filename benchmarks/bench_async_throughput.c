// Async client throughput benchmark
// Tests maximum requests/second with connection pooling

#include "../include/cwebhttp_async.h"
#include "../include/cwebhttp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#define sleep(x) Sleep((x) * 1000)
#else
#include <unistd.h>
#include <sys/time.h>
#endif

// Configuration
#define TEST_DURATION_SEC 10
#define CONCURRENT_REQUESTS 100
#define POOL_SIZE 50
#define TEST_URL "http://httpbin.org/get"

static int requests_sent = 0;
static int requests_completed = 0;
static int requests_failed = 0;
static int status_200 = 0;
static int status_other = 0;
static volatile int benchmark_running = 1;
static cwh_loop_t *g_loop = NULL;

#ifdef _WIN32
static double get_time_sec(void)
{
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / freq.QuadPart;
}
#else
static double get_time_sec(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}
#endif

// Request callback
static void request_callback(cwh_response_t *res, cwh_error_t err, void *data)
{
    (void)data;

    if (err == CWH_OK && res)
    {
        requests_completed++;
        if (res->status == 200)
        {
            status_200++;
        }
        else
        {
            status_other++;
        }
    }
    else
    {
        requests_failed++;
    }

    // Print progress
    if ((requests_completed + requests_failed) % 100 == 0)
    {
        printf("Progress: %d sent, %d completed, %d failed\r",
               requests_sent, requests_completed, requests_failed);
        fflush(stdout);
    }
}

int main(void)
{
    printf("=== Async Client Throughput Benchmark ===\n");
    printf("Target: %s\n", TEST_URL);
    printf("Duration: %d seconds\n", TEST_DURATION_SEC);
    printf("Concurrent requests: %d\n", CONCURRENT_REQUESTS);
    printf("Connection pool size: %d\n\n", POOL_SIZE);

#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    // Create event loop
    g_loop = cwh_loop_new();
    if (!g_loop)
    {
        printf("Failed to create event loop\n");
        return 1;
    }

    printf("Event loop backend: %s\n", cwh_loop_backend(g_loop));

    // Initialize connection pool
    cwh_async_pool_init(POOL_SIZE, 60);

    printf("Starting benchmark...\n\n");

    double start_time = get_time_sec();
    double last_stats_time = start_time;
    int last_completed = 0;

    // Main benchmark loop
    while (get_time_sec() - start_time < TEST_DURATION_SEC)
    {
        // Maintain concurrent request level
        int active = requests_sent - requests_completed - requests_failed;
        while (active < CONCURRENT_REQUESTS)
        {
            cwh_async_get(g_loop, TEST_URL, request_callback, NULL);
            requests_sent++;
            active++;
        }

        // Process events
        cwh_loop_run_once(g_loop, 1);

        // Print stats every second
        double current_time = get_time_sec();
        if (current_time - last_stats_time >= 1.0)
        {
            int completed_delta = requests_completed - last_completed;
            printf("t=%.0fs: %d req/s (total: %d sent, %d completed, %d failed)\n",
                   current_time - start_time, completed_delta,
                   requests_sent, requests_completed, requests_failed);
            last_stats_time = current_time;
            last_completed = requests_completed;
        }
    }

    // Wait for remaining requests
    printf("\nWaiting for remaining requests to complete...\n");
    while (requests_completed + requests_failed < requests_sent)
    {
        cwh_loop_run_once(g_loop, 100);
    }

    double end_time = get_time_sec();
    double total_time = end_time - start_time;

    // Print final results
    printf("\n=== Benchmark Results ===\n");
    printf("Duration: %.2f seconds\n", total_time);
    printf("Total requests sent: %d\n", requests_sent);
    printf("Completed: %d\n", requests_completed);
    printf("Failed: %d\n", requests_failed);
    printf("Success rate: %.1f%%\n", 100.0 * requests_completed / requests_sent);
    printf("\nStatus codes:\n");
    printf("  200 OK: %d\n", status_200);
    printf("  Other: %d\n", status_other);
    printf("\nThroughput: %.2f requests/second\n", requests_completed / total_time);

    // Get pool stats
    int pool_active, pool_total;
    cwh_async_pool_stats(&pool_active, &pool_total);
    printf("\nConnection pool:\n");
    printf("  Active: %d\n", pool_active);
    printf("  Total: %d\n", pool_total);
    printf("  Reuse rate: %.1f%%\n",
           100.0 * (requests_completed - pool_total) / requests_completed);

    // Performance evaluation
    printf("\n=== Performance Evaluation ===\n");
    double rps = requests_completed / total_time;
    if (rps >= 1000)
    {
        printf("✅ EXCELLENT: %.0f req/s (>= 1000)\n", rps);
    }
    else if (rps >= 500)
    {
        printf("✅ GOOD: %.0f req/s (>= 500)\n", rps);
    }
    else if (rps >= 100)
    {
        printf("⚠️  ACCEPTABLE: %.0f req/s (>= 100)\n", rps);
    }
    else
    {
        printf("❌ POOR: %.0f req/s (< 100)\n", rps);
    }

    // Cleanup
    cwh_async_pool_shutdown();
    cwh_loop_free(g_loop);

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}
