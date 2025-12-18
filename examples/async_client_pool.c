// Async HTTP client with connection pooling example
// Demonstrates efficient connection reuse for multiple requests

#include "../include/cwebhttp_async.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int requests_completed = 0;
static int total_requests = 10;

// Response callback
void on_response(cwh_response_t *res, cwh_error_t err, void *data)
{
    int request_id = *(int*)data;
    
    if (err != CWH_OK) {
        printf("Request %d failed: %d\n", request_id, err);
    } else {
        printf("Request %d: HTTP %d (%d bytes)\n", 
               request_id, res->status, (int)res->body_len);
    }
    
    requests_completed++;
    
    // Show pool stats after each request
    int active, total;
    cwh_async_pool_stats(&active, &total);
    printf("  Pool: %d active, %d total connections\n", active, total);
    
    free(data);
}

int main(void)
{
    printf("=== Async HTTP Client with Connection Pooling ===\n\n");
    
    // Initialize connection pool (10 connections, 60 second timeout)
    cwh_async_pool_init(10, 60);
    
    // Create event loop
    cwh_loop_t *loop = cwh_loop_new();
    if (!loop) {
        printf("Failed to create event loop\n");
        return 1;
    }
    
    printf("Backend: %s\n", cwh_loop_backend(loop));
    printf("Making %d requests to httpbin.org...\n\n", total_requests);
    
    // Make multiple requests to the same host (will reuse connections)
    for (int i = 0; i < total_requests; i++) {
        int *request_id = malloc(sizeof(int));
        *request_id = i + 1;
        
        // Alternate between different endpoints on same host
        const char *urls[] = {
            "http://httpbin.org/get",
            "http://httpbin.org/headers", 
            "http://httpbin.org/user-agent",
            "http://httpbin.org/ip"
        };
        
        const char *url = urls[i % 4];
        cwh_async_get(loop, url, on_response, request_id);
    }
    
    // Run event loop until all requests complete
    while (requests_completed < total_requests) {
        if (cwh_loop_run_once(loop, 1000) < 0) {
            printf("Event loop error\n");
            break;
        }
        
        // Cleanup expired connections periodically
        cwh_async_pool_cleanup();
    }
    
    // Final pool statistics
    int active, total;
    cwh_async_pool_stats(&active, &total);
    printf("\nFinal pool stats: %d active, %d total connections\n", active, total);
    
    // Cleanup
    cwh_async_pool_shutdown();
    cwh_loop_free(loop);
    
    printf("All requests completed!\n");
    return 0;
}