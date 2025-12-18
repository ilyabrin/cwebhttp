// async_echo_server.c - Example async echo server using event loop
// Demonstrates async I/O API (Linux/macOS only - requires epoll/kqueue)

#include "cwebhttp_async.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_CLIENTS 1024
#define BUFFER_SIZE 4096

// Client connection state
typedef struct
{
    int fd;
    char buffer[BUFFER_SIZE];
    size_t buffer_len;
} client_t;

static client_t clients[MAX_CLIENTS];
static int server_fd = -1;

// Accept new client connections
void on_accept(cwh_loop_t *loop, int fd, int events, void *data)
{
    (void)events;
    (void)data;

    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    int client_fd = accept(fd, (struct sockaddr *)&client_addr, &addr_len);
    if (client_fd < 0)
    {
        perror("accept");
        return;
    }

    // Set non-blocking
    if (cwh_set_nonblocking(client_fd) < 0)
    {
        close(client_fd);
        return;
    }

    // Find free slot
    int slot = -1;
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].fd == -1)
        {
            slot = i;
            break;
        }
    }

    if (slot < 0)
    {
        printf("Too many clients, rejecting connection\n");
        close(client_fd);
        return;
    }

    // Initialize client
    clients[slot].fd = client_fd;
    clients[slot].buffer_len = 0;

    // Add to event loop (read events)
    cwh_loop_add(loop, client_fd, CWH_EVENT_READ, on_client_read, &clients[slot]);

    printf("Client connected: fd=%d, slot=%d\n", client_fd, slot);
}

// Handle client read events
void on_client_read(cwh_loop_t *loop, int fd, int events, void *data)
{
    client_t *client = (client_t *)data;

    if (events & CWH_EVENT_ERROR)
    {
        printf("Client error: fd=%d\n", fd);
        cwh_loop_del(loop, fd);
        close(fd);
        client->fd = -1;
        return;
    }

    // Read data
    char buffer[BUFFER_SIZE];
    ssize_t n = recv(fd, buffer, sizeof(buffer), 0);

    if (n <= 0)
    {
        if (n == 0)
        {
            printf("Client disconnected: fd=%d\n", fd);
        }
        else
        {
            perror("recv");
        }
        cwh_loop_del(loop, fd);
        close(fd);
        client->fd = -1;
        return;
    }

    // Echo back immediately (simple echo)
    ssize_t sent = send(fd, buffer, n, 0);
    if (sent < 0)
    {
        perror("send");
        cwh_loop_del(loop, fd);
        close(fd);
        client->fd = -1;
        return;
    }

    printf("Echoed %zd bytes to client fd=%d\n", sent, fd);
}

int main(void)
{
    printf("=== Async Echo Server ===\n");
    printf("Platform: Linux/macOS only (epoll/kqueue)\n\n");

    // Initialize client slots
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        clients[i].fd = -1;
    }

    // Create event loop
    cwh_loop_t *loop = cwh_loop_new();
    if (!loop)
    {
        fprintf(stderr, "Failed to create event loop\n");
        return 1;
    }

    printf("Event loop backend: %s\n", cwh_loop_backend(loop));

    // Create listening socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("socket");
        cwh_loop_free(loop);
        return 1;
    }

    // Set SO_REUSEADDR
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Set non-blocking
    if (cwh_set_nonblocking(server_fd) < 0)
    {
        perror("set_nonblocking");
        close(server_fd);
        cwh_loop_free(loop);
        return 1;
    }

    // Bind to port 8080
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(8080);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        close(server_fd);
        cwh_loop_free(loop);
        return 1;
    }

    // Listen
    if (listen(server_fd, 128) < 0)
    {
        perror("listen");
        close(server_fd);
        cwh_loop_free(loop);
        return 1;
    }

    printf("Listening on port 8080...\n");
    printf("Test with: telnet localhost 8080\n\n");

    // Add server socket to event loop
    if (cwh_loop_add(loop, server_fd, CWH_EVENT_READ, on_accept, NULL) < 0)
    {
        fprintf(stderr, "Failed to add server to event loop\n");
        close(server_fd);
        cwh_loop_free(loop);
        return 1;
    }

    // Run event loop
    printf("Event loop running (Ctrl+C to stop)...\n");
    cwh_loop_run(loop);

    // Cleanup
    printf("\nShutting down...\n");
    close(server_fd);
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].fd >= 0)
        {
            close(clients[i].fd);
        }
    }
    cwh_loop_free(loop);

    return 0;
}

#else
// Windows placeholder
int main(void)
{
    printf("Async echo server is not supported on Windows (epoll not available)\n");
    printf("Please run on Linux or macOS with epoll/kqueue support\n");
    return 1;
}
#endif
