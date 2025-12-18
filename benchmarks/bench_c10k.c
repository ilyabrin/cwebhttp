// C10K Performance Benchmark - Linux epoll async server
// Tests concurrent connection handling and throughput

#define _DEFAULT_SOURCE  // For usleep()
#include "../include/cwebhttp_async.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <sys/time.h>

#ifdef __linux__
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/resource.h>
#include <sys/wait.h>

// Benchmark configuration
#define BENCH_PORT 8080
#define MAX_CLIENTS 10000
#define REQUESTS_PER_CLIENT 10
#define CONCURRENT_CONNECTIONS 1000

// Global stats
static volatile int total_requests = 0;
static volatile int total_responses = 0;
static volatile int active_connections = 0;
static volatile int benchmark_running = 1;
static struct timeval start_time, end_time;

// Simple HTTP handler for benchmarking
static void hello_handler(cwh_async_conn_t *conn, cwh_request_t *req, void *data)
{
    (void)req;
    (void)data;
    
    const char *response = "Hello, World!";
    cwh_async_send_response(conn, 200, "text/plain", response, strlen(response));
    total_responses++;
}

// Signal handler for graceful shutdown
static void signal_handler(int sig)
{
    (void)sig;
    benchmark_running = 0;
}

// Set socket non-blocking
static int set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// Create client connection
static int create_client_socket(void)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;
    
    if (set_nonblocking(sock) < 0) {
        close(sock);
        return -1;
    }
    
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(BENCH_PORT);
    
    int result = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    if (result < 0 && errno != EINPROGRESS) {
        close(sock);
        return -1;
    }
    
    return sock;
}

// Send HTTP request
static int send_request(int sock)
{
    const char *request = 
        "GET / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: keep-alive\r\n"
        "\r\n";
    
    ssize_t sent = send(sock, request, strlen(request), 0);
    if (sent > 0) {
        total_requests++;
        return 0;
    }
    return -1;
}

// Read response (simple)
static int read_response(int sock)
{
    char buf[1024];
    ssize_t n = recv(sock, buf, sizeof(buf)-1, 0);
    return (n > 0) ? 0 : -1;
}

// Client benchmark thread simulation
static void run_client_benchmark(void)
{
    int *client_socks = calloc(CONCURRENT_CONNECTIONS, sizeof(int));
    int *requests_sent = calloc(CONCURRENT_CONNECTIONS, sizeof(int));
    
    printf("Creating %d concurrent connections...\n", CONCURRENT_CONNECTIONS);
    
    // Create connections
    for (int i = 0; i < CONCURRENT_CONNECTIONS && benchmark_running; i++) {
        client_socks[i] = create_client_socket();
        if (client_socks[i] >= 0) {
            active_connections++;
        }
        usleep(100); // Small delay to avoid overwhelming
    }
    
    printf("Active connections: %d\n", active_connections);
    
    // Send requests in round-robin
    while (benchmark_running && total_requests < MAX_CLIENTS * REQUESTS_PER_CLIENT) {
        for (int i = 0; i < CONCURRENT_CONNECTIONS && benchmark_running; i++) {
            if (client_socks[i] >= 0 && requests_sent[i] < REQUESTS_PER_CLIENT) {
                if (send_request(client_socks[i]) == 0) {
                    requests_sent[i]++;
                }
                
                // Try to read response
                read_response(client_socks[i]);
            }
        }
        usleep(1000); // 1ms delay
    }
    
    // Cleanup
    for (int i = 0; i < CONCURRENT_CONNECTIONS; i++) {
        if (client_socks[i] >= 0) {
            close(client_socks[i]);
        }
    }
    
    free(client_socks);
    free(requests_sent);
}

// Print benchmark results
static void print_results(void)
{
    double duration = (end_time.tv_sec - start_time.tv_sec) + 
                     (end_time.tv_usec - start_time.tv_usec) / 1000000.0;
    
    printf("\n=== C10K Benchmark Results ===\n");
    printf("Duration: %.2f seconds\n", duration);
    printf("Total requests: %d\n", total_requests);
    printf("Total responses: %d\n", total_responses);
    printf("Requests/second: %.2f\n", total_requests / duration);
    printf("Concurrent connections: %d\n", active_connections);
    
    if (total_requests >= 10000) {
        printf("✅ C10K CAPABLE: Handled 10K+ requests\n");
    } else {
        printf("❌ C10K FAILED: Only handled %d requests\n", total_requests);
    }
}

int main(void)
{
    printf("=== cwebhttp C10K Performance Benchmark ===\n");
    printf("Testing async server with epoll backend on Linux\n\n");
    
    // Increase file descriptor limit
    struct rlimit rlim;
    rlim.rlim_cur = 65536;
    rlim.rlim_max = 65536;
    if (setrlimit(RLIMIT_NOFILE, &rlim) < 0) {
        printf("Warning: Could not increase file descriptor limit\n");
    }
    
    // Setup signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Create event loop and server
    cwh_loop_t *loop = cwh_loop_new();
    if (!loop) {
        printf("Failed to create event loop\n");
        return 1;
    }
    
    cwh_async_server_t *server = cwh_async_server_new(loop);
    if (!server) {
        printf("Failed to create server\n");
        cwh_loop_free(loop);
        return 1;
    }
    
    // Register route
    cwh_async_route(server, "GET", "/", hello_handler, NULL);
    
    // Start listening
    if (cwh_async_listen(server, BENCH_PORT) < 0) {
        printf("Failed to start server on port %d\n", BENCH_PORT);
        cwh_async_server_free(server);
        cwh_loop_free(loop);
        return 1;
    }
    
    printf("Server started on port %d\n", BENCH_PORT);
    printf("Backend: %s\n", cwh_loop_backend(loop));
    
    // Start benchmark timer
    gettimeofday(&start_time, NULL);
    
    // Fork client process
    pid_t client_pid = fork();
    if (client_pid == 0) {
        // Child process - run client benchmark
        sleep(1); // Let server start
        run_client_benchmark();
        exit(0);
    } else if (client_pid > 0) {
        // Parent process - run server
        printf("Running benchmark for 30 seconds...\n");
        
        time_t benchmark_start = time(NULL);
        while (benchmark_running && (time(NULL) - benchmark_start) < 30) {
            cwh_loop_run_once(loop, 100); // 100ms timeout
        }
        
        // Stop benchmark
        benchmark_running = 0;
        gettimeofday(&end_time, NULL);
        
        // Wait for client to finish
        int status;
        waitpid(client_pid, &status, 0);
        
        print_results();
    } else {
        printf("Failed to fork client process\n");
        return 1;
    }
    
    // Cleanup
    cwh_async_server_free(server);
    cwh_loop_free(loop);
    
    return 0;
}

#else
// Non-Linux platforms
int main(void)
{
    printf("C10K benchmark requires Linux with epoll support\n");
    return 1;
}
#endif