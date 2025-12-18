/*
 * Benchmark Client Example
 * Load testing tool for HTTP servers
 * 
 * Features:
 * - Concurrent async requests
 * - Latency measurement
 * - Throughput calculation
 * - Progress reporting
 * 
 * Compile: make build/examples/benchmark_client
 * Run: ./build/examples/benchmark_client http://localhost:8080/ 1000 100
 *      (URL, total requests, concurrency)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "cwebhttp_async.h"

#undef CWH_MEMCHECK_ENABLED

#ifdef _WIN32
#include <windows.h>
#define sleep(s) Sleep((s) * 1000)
#else
#include <unistd.h>
#include <sys/time.h>
#endif

/* Benchmark statistics */
typedef struct {
    int total_requests;
    int completed;
    int failed;
    int active;
    double total_time_ms;
    double min_time_ms;
    double max_time_ms;
    time_t start_time;
} BenchStats;

static BenchStats stats = {0};

/* Forward declaration */
void request_callback(cwh_response_t *res, cwh_error_t err, void *user_data);

/* Get current time in milliseconds */
double get_time_ms(void) {
#ifdef _WIN32
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart * 1000.0 / (double)freq.QuadPart;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
#endif
}

/* Request callback */
void request_callback(cwh_response_t *res, cwh_error_t err, void *user_data) {
    double *start_time = (double*)user_data;
    double elapsed = get_time_ms() - *start_time;
    
    stats.active--;
    
    if (err == CWH_OK && res) {
        stats.completed++;
        stats.total_time_ms += elapsed;
        
        if (stats.min_time_ms == 0 || elapsed < stats.min_time_ms) {
            stats.min_time_ms = elapsed;
        }
        if (elapsed > stats.max_time_ms) {
            stats.max_time_ms = elapsed;
        }
    } else {
        stats.failed++;
    }
    
    free(start_time);
    
    /* Print progress every 100 requests */
    if ((stats.completed + stats.failed) % 100 == 0) {
        printf("\rProgress: %d/%d completed, %d failed, %d active",
               stats.completed, stats.total_requests, stats.failed, stats.active);
        fflush(stdout);
    }
}

/* Print final statistics */
void print_stats(void) {
    time_t end_time = time(NULL);
    double duration_s = difftime(end_time, stats.start_time);
    
    printf("\n\n========================================\n");
    printf("Benchmark Results\n");
    printf("========================================\n\n");
    
    printf("Total Requests:   %d\n", stats.total_requests);
    printf("Completed:        %d (%.1f%%)\n", 
           stats.completed, 
           (double)stats.completed / stats.total_requests * 100);
    printf("Failed:           %d (%.1f%%)\n", 
           stats.failed,
           (double)stats.failed / stats.total_requests * 100);
    printf("\n");
    
    printf("Duration:         %.2f seconds\n", duration_s);
    printf("Requests/sec:     %.2f\n", stats.completed / duration_s);
    printf("\n");
    
    if (stats.completed > 0) {
        double avg_time = stats.total_time_ms / stats.completed;
        printf("Latency (ms):\n");
        printf("  Min:            %.2f\n", stats.min_time_ms);
        printf("  Max:            %.2f\n", stats.max_time_ms);
        printf("  Avg:            %.2f\n", avg_time);
    }
    
    printf("\n========================================\n");
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Usage: %s <url> <total_requests> <concurrency>\n", argv[0]);
        printf("Example: %s http://localhost:8080/ 1000 100\n", argv[0]);
        return 1;
    }
    
    const char *url = argv[1];
    int total_requests = atoi(argv[2]);
    int concurrency = atoi(argv[3]);
    
    if (total_requests <= 0 || concurrency <= 0) {
        fprintf(stderr, "Error: requests and concurrency must be positive\n");
        return 1;
    }
    
    if (concurrency > total_requests) {
        concurrency = total_requests;
    }
    
    printf("========================================\n");
    printf("HTTP Benchmark Client\n");
    printf("========================================\n\n");
    printf("URL:              %s\n", url);
    printf("Total Requests:   %d\n", total_requests);
    printf("Concurrency:      %d\n", concurrency);
    printf("\n");
    
    /* Initialize stats */
    stats.total_requests = total_requests;
    stats.start_time = time(NULL);
    
    /* Create event loop */
    cwh_loop_t *loop = cwh_loop_new();
    if (!loop) {
        fprintf(stderr, "Failed to create event loop\n");
        return 1;
    }
    
    printf("✓ Event loop created\n");
    printf("\n========================================\n");
    printf("Starting benchmark...\n");
    printf("========================================\n\n");
    
    /* Launch initial batch of requests */
    int sent = 0;
    for (int i = 0; i < concurrency && sent < total_requests; i++) {
        double *start_time = malloc(sizeof(double));
        *start_time = get_time_ms();
        
        cwh_async_get(loop, url, request_callback, start_time);
        stats.active++;
        sent++;
    }
    
    /* Launch remaining requests as others complete */
    while (stats.completed + stats.failed < total_requests) {
        /* Run one iteration of event loop */
        cwh_loop_run_once(loop, 10);
        
        /* Launch more requests if needed */
        while (stats.active < concurrency && sent < total_requests) {
            double *start_time = malloc(sizeof(double));
            *start_time = get_time_ms();
            
            cwh_async_get(loop, url, request_callback, start_time);
            stats.active++;
            sent++;
        }
    }
    
    /* Final iteration to ensure all callbacks complete */
    while (stats.active > 0) {
        cwh_loop_run_once(loop, 10);
    }
    
    /* Print results */
    print_stats();
    
    /* Cleanup */
    cwh_loop_free(loop);
    
    return (stats.failed > 0) ? 1 : 0;
}
