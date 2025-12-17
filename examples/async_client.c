// async_client.c - Example demonstrating async HTTP client
// Non-blocking HTTP requests using event loop

#include "../include/cwebhttp_async.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <signal.h>
#endif

// Global loop for signal handling
static cwh_loop_t *g_loop = NULL;
static int completed_requests = 0;
static int total_requests = 0;

// Response callback
void on_response(cwh_response_t *res, cwh_error_t err, void *data)
{
    const char *url = (const char *)data;

    printf("\n========================================\n");
    printf("Response from: %s\n", url);
    printf("========================================\n");

    if (err != CWH_OK)
    {
        printf("Error: %d\n", err);
    }
    else
    {
        printf("Status: %d\n", res->status);
        printf("Body (%lu bytes):\n%.*s\n",
               (unsigned long)res->body_len,
               (int)(res->body_len > 500 ? 500 : res->body_len),
               res->body);

        if (res->body_len > 500)
        {
            printf("... (truncated)\n");
        }
    }

    completed_requests++;
    printf("\nCompleted %d/%d requests\n", completed_requests, total_requests);

    // Stop loop when all requests complete
    if (completed_requests >= total_requests && g_loop)
    {
        printf("\nAll requests complete. Stopping event loop...\n");
        cwh_loop_stop(g_loop);
    }
}

// Example 1: Simple GET request
void example_simple_get(cwh_loop_t *loop)
{
    printf("=== Example 1: Simple GET Request ===\n\n");

    total_requests = 1;
    completed_requests = 0;

    const char *url = "http://example.com/";
    printf("Making async GET request to: %s\n", url);

    cwh_async_get(loop, url, on_response, (void *)url);

    printf("Request initiated. Running event loop...\n\n");
    cwh_loop_run(loop);
}

// Example 2: Multiple concurrent requests
void example_concurrent_requests(cwh_loop_t *loop)
{
    printf("\n=== Example 2: Multiple Concurrent Requests ===\n\n");

    const char *urls[] = {
        "http://example.com/",
        "http://www.example.org/",
        "http://httpbin.org/get",
        NULL};

    total_requests = 0;
    completed_requests = 0;

    // Count URLs
    for (int i = 0; urls[i]; i++)
    {
        total_requests++;
    }

    printf("Making %d concurrent async requests:\n", total_requests);
    for (int i = 0; urls[i]; i++)
    {
        printf("  %d. %s\n", i + 1, urls[i]);
        cwh_async_get(loop, urls[i], on_response, (void *)urls[i]);
    }

    printf("\nAll requests initiated. Running event loop...\n\n");
    cwh_loop_run(loop);
}

// POST callback
void on_post_response(cwh_response_t *res, cwh_error_t err, void *data)
{
    (void)data;
    printf("\n========================================\n");
    printf("POST Response\n");
    printf("========================================\n");

    if (err != CWH_OK)
    {
        printf("Error: %d\n", err);
    }
    else
    {
        printf("Status: %d\n", res->status);
        printf("Body:\n%.*s\n", (int)res->body_len, res->body);
    }

    completed_requests++;
    if (completed_requests >= total_requests && g_loop)
    {
        cwh_loop_stop(g_loop);
    }
}

// Example 3: POST request with body
void example_post_request(cwh_loop_t *loop)
{
    printf("\n=== Example 3: POST Request with JSON Body ===\n\n");

    total_requests = 1;
    completed_requests = 0;

    const char *url = "http://httpbin.org/post";
    const char *json_body = "{\"name\":\"John Doe\",\"email\":\"john@example.com\"}";

    printf("Making async POST request to: %s\n", url);
    printf("Body: %s\n", json_body);

    cwh_async_post(loop, url, json_body, strlen(json_body), on_post_response, NULL);

    printf("\nRequest initiated. Running event loop...\n\n");
    cwh_loop_run(loop);
}

int main(int argc, char *argv[])
{
#ifdef _WIN32
    // Initialize Winsock on Windows
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        fprintf(stderr, "Failed to initialize Winsock\n");
        return 1;
    }
#else
    // Ignore SIGPIPE on Unix
    signal(SIGPIPE, SIG_IGN);
#endif

    printf("cwebhttp Async Client Example\n");
    printf("==============================\n\n");

    // Create event loop
    cwh_loop_t *loop = cwh_loop_new();
    if (!loop)
    {
        fprintf(stderr, "Failed to create event loop\n");
        return 1;
    }

    g_loop = loop;

    printf("Event loop backend: %s\n\n", cwh_loop_backend(loop));

    // Run examples based on command line argument
    if (argc > 1)
    {
        int example = atoi(argv[1]);
        switch (example)
        {
        case 1:
            example_simple_get(loop);
            break;
        case 2:
            example_concurrent_requests(loop);
            break;
        case 3:
            example_post_request(loop);
            break;
        default:
            printf("Unknown example: %d\n", example);
            printf("Usage: %s [1|2|3]\n", argv[0]);
            printf("  1 - Simple GET request\n");
            printf("  2 - Multiple concurrent requests\n");
            printf("  3 - POST request with JSON body\n");
            break;
        }
    }
    else
    {
        // Run all examples
        example_simple_get(loop);
        example_concurrent_requests(loop);
        example_post_request(loop);
    }

    // Cleanup
    cwh_loop_free(loop);

#ifdef _WIN32
    WSACleanup();
#endif

    printf("\nDone!\n");
    return 0;
}
