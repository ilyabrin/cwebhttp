// Latency profiling benchmark for async HTTP client
// Measures request latency distribution (p50, p95, p99, p999)

#include "../include/cwebhttp_async.h"
#include "../include/cwebhttp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#ifdef _WIN32
#include <windows.h>
#define sleep(x) Sleep((x) * 1000)
#else
#include <unistd.h>
#include <sys/time.h>
#endif

// Configuration
#define TOTAL_REQUESTS 1000
#define CONCURRENT_REQUESTS 50
#define TEST_URL "http://httpbin.org/delay/0"

// Latency measurement structure
typedef struct {
    int64_t start_us;
    int64_t end_us;
    int64_t duration_us;
    int status_code;
} latency_sample_t;

static latency_sample_t *samples = NULL;
static int samples_count = 0;
static int samples_capacity = 0;
static int requests_completed = 0;
static int requests_failed = 0;
static cwh_loop_t *g_loop = NULL;

// Get current time in microseconds
static int64_t get_time_us(void) {
#ifdef _WIN32
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (counter.QuadPart * 1000000LL) / freq.QuadPart;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000000LL + tv.tv_usec;
#endif
}

// Record latency sample
static void record_sample(int64_t start_us, int64_t end_us, int status_code) {
    if (samples_count >= samples_capacity) {
        samples_capacity = (samples_capacity == 0) ? 1024 : samples_capacity * 2;
        samples = realloc(samples, samples_capacity * sizeof(latency_sample_t));
    }
    
    samples[samples_count].start_us = start_us;
    samples[samples_count].end_us = end_us;
    samples[samples_count].duration_us = end_us - start_us;
    samples[samples_count].status_code = status_code;
    samples_count++;
}

// Comparison function for qsort
static int compare_latencies(const void *a, const void *b) {
    int64_t diff = ((latency_sample_t*)a)->duration_us - ((latency_sample_t*)b)->duration_us;
    return (diff < 0) ? -1 : (diff > 0) ? 1 : 0;
}

// Calculate percentile
static int64_t calculate_percentile(double percentile) {
    if (samples_count == 0) return 0;
    
    int index = (int)((percentile / 100.0) * samples_count);
    if (index >= samples_count) index = samples_count - 1;
    return samples[index].duration_us;
}

// Print statistics
static void print_statistics(void) {
    if (samples_count == 0) {
        printf("No samples recorded\n");
        return;
    }
    
    // Sort samples by latency
    qsort(samples, samples_count, sizeof(latency_sample_t), compare_latencies);
    
    // Calculate statistics
    int64_t total = 0;
    int64_t min = samples[0].duration_us;
    int64_t max = samples[samples_count-1].duration_us;
    
    for (int i = 0; i < samples_count; i++) {
        total += samples[i].duration_us;
    }
    
    double mean = (double)total / samples_count;
    
    printf("\n=== Latency Distribution ===\n");
    printf("Total requests: %d\n", TOTAL_REQUESTS);
    printf("Completed: %d\n", requests_completed);
    printf("Failed: %d\n", requests_failed);
    printf("Samples: %d\n\n", samples_count);
    
    printf("Latency (microseconds):\n");
    printf("  Min:    %lld μs (%.2f ms)\n", min, min / 1000.0);
    printf("  Mean:   %.0f μs (%.2f ms)\n", mean, mean / 1000.0);
    printf("  Median: %lld μs (%.2f ms)\n", 
           calculate_percentile(50), calculate_percentile(50) / 1000.0);
    printf("  p95:    %lld μs (%.2f ms)\n", 
           calculate_percentile(95), calculate_percentile(95) / 1000.0);
    printf("  p99:    %lld μs (%.2f ms)\n", 
           calculate_percentile(99), calculate_percentile(99) / 1000.0);
    printf("  p999:   %lld μs (%.2f ms)\n", 
           calculate_percentile(99.9), calculate_percentile(99.9) / 1000.0);
    printf("  Max:    %lld μs (%.2f ms)\n\n", max, max / 1000.0);
    
    // Latency buckets
    printf("Latency buckets:\n");
    int bucket_10ms = 0, bucket_50ms = 0, bucket_100ms = 0, 
        bucket_500ms = 0, bucket_1s = 0, bucket_over = 0;
    
    for (int i = 0; i < samples_count; i++) {
        int64_t lat_ms = samples[i].duration_us / 1000;
        if (lat_ms < 10) bucket_10ms++;
        else if (lat_ms < 50) bucket_50ms++;
        else if (lat_ms < 100) bucket_100ms++;
        else if (lat_ms < 500) bucket_500ms++;
        else if (lat_ms < 1000) bucket_1s++;
        else bucket_over++;
    }
    
    printf("  < 10ms:    %d (%.1f%%)\n", bucket_10ms, 100.0 * bucket_10ms / samples_count);
    printf("  10-50ms:   %d (%.1f%%)\n", bucket_50ms, 100.0 * bucket_50ms / samples_count);
    printf("  50-100ms:  %d (%.1f%%)\n", bucket_100ms, 100.0 * bucket_100ms / samples_count);
    printf("  100-500ms: %d (%.1f%%)\n", bucket_500ms, 100.0 * bucket_500ms / samples_count);
    printf("  500ms-1s:  %d (%.1f%%)\n", bucket_1s, 100.0 * bucket_1s / samples_count);
    printf("  > 1s:      %d (%.1f%%)\n", bucket_over, 100.0 * bucket_over / samples_count);
}

// Request callback
static void request_callback(cwh_response_t *res, cwh_error_t err, void *data) {
    int64_t end_us = get_time_us();
    int64_t *start_us_ptr = (int64_t*)data;
    
    if (err == CWH_OK && res) {
        record_sample(*start_us_ptr, end_us, res->status);
        requests_completed++;
        
        if (requests_completed % 100 == 0) {
            printf("Progress: %d/%d requests completed\r", requests_completed, TOTAL_REQUESTS);
            fflush(stdout);
        }
    } else {
        requests_failed++;
    }
    
    free(start_us_ptr);
    
    // Stop loop when all requests complete
    if (requests_completed + requests_failed >= TOTAL_REQUESTS) {
        cwh_loop_stop(g_loop);
    }
}

int main(void) {
    printf("=== Async Client Latency Benchmark ===\n");
    printf("Target: %s\n", TEST_URL);
    printf("Total requests: %d\n", TOTAL_REQUESTS);
    printf("Concurrent: %d\n\n", CONCURRENT_REQUESTS);
    
    // Initialize Winsock on Windows
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    
    // Create event loop
    g_loop = cwh_loop_new();
    if (!g_loop) {
        printf("Failed to create event loop\n");
        return 1;
    }
    
    printf("Event loop backend: %s\n", cwh_loop_backend(g_loop));
    printf("Starting benchmark...\n\n");
    
    // Initialize connection pool
    cwh_async_pool_init(CONCURRENT_REQUESTS, 60);
    
    int64_t benchmark_start = get_time_us();
    
    // Send requests in batches
    int requests_sent = 0;
    int active = 0;
    
    while (requests_sent < TOTAL_REQUESTS) {
        // Send batch of concurrent requests
        while (active < CONCURRENT_REQUESTS && requests_sent < TOTAL_REQUESTS) {
            int64_t *start_time = malloc(sizeof(int64_t));
            *start_time = get_time_us();
            
            cwh_async_get(g_loop, TEST_URL, request_callback, start_time);
            requests_sent++;
            active++;
        }
        
        // Process some events
        cwh_loop_run_once(g_loop, 10);
        active = CONCURRENT_REQUESTS - (requests_completed + requests_failed - (requests_sent - CONCURRENT_REQUESTS));
    }
    
    // Process remaining responses
    printf("Waiting for remaining responses...\n");
    while (requests_completed + requests_failed < TOTAL_REQUESTS) {
        cwh_loop_run_once(g_loop, 100);
    }
    
    int64_t benchmark_end = get_time_us();
    double total_time_sec = (benchmark_end - benchmark_start) / 1000000.0;
    
    // Print results
    print_statistics();
    
    printf("Total benchmark time: %.2f seconds\n", total_time_sec);
    printf("Throughput: %.2f requests/sec\n", requests_completed / total_time_sec);
    
    // Evaluate performance
    int64_t p99 = calculate_percentile(99);
    printf("\n=== Performance Evaluation ===\n");
    if (p99 < 100000) { // < 100ms
        printf("✅ EXCELLENT: p99 < 100ms\n");
    } else if (p99 < 500000) { // < 500ms
        printf("✅ GOOD: p99 < 500ms\n");
    } else if (p99 < 1000000) { // < 1s
        printf("⚠️  ACCEPTABLE: p99 < 1s\n");
    } else {
        printf("❌ POOR: p99 > 1s\n");
    }
    
    // Cleanup
    cwh_async_pool_shutdown();
    cwh_loop_free(g_loop);
    free(samples);
    
#ifdef _WIN32
    WSACleanup();
#endif
    
    return 0;
}
