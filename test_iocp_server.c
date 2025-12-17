// test_iocp_server.c - Test Windows IOCP Async Server
// Minimal test to verify AcceptEx completions are delivered

#include "include/cwebhttp_async.h"
#include "include/cwebhttp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#error "This test is Windows-only"
#endif

// Simple request handler
void handle_request(cwh_async_conn_t *conn, cwh_request_t *req, void *data)
{
    (void)data;
    printf("[HANDLER] Got request: %s %s\n",
           req->method_str, req->path);

    const char *body = "Hello, IOCP!\n";

    cwh_async_send_response(conn, 200, "text/plain", body, strlen(body));
}

int main(void)
{
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
    {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }

    printf("=== IOCP Async Server Test ===\n");
    printf("This test verifies that AcceptEx completions are delivered\n\n");

    // Create event loop
    cwh_loop_t *loop = cwh_loop_new();
    if (!loop)
    {
        fprintf(stderr, "Failed to create event loop\n");
        WSACleanup();
        return 1;
    }

    printf("Event loop backend: %s\n", cwh_loop_backend(loop));

    // Verify we're using IOCP
    const char *backend = cwh_loop_backend(loop);
    if (strstr(backend, "IOCP") == NULL && strstr(backend, "iocp") == NULL)
    {
        fprintf(stderr, "ERROR: Not using IOCP backend! Got: %s\n", backend);
        cwh_loop_free(loop);
        WSACleanup();
        return 1;
    }
    printf("✓ IOCP backend confirmed\n\n");

    // Create server
    cwh_async_server_t *server = cwh_async_server_new(loop);
    if (!server)
    {
        fprintf(stderr, "Failed to create server\n");
        cwh_loop_free(loop);
        WSACleanup();
        return 1;
    }
    printf("✓ Server created\n");

    // Add route
    cwh_async_route(server, "GET", "/", handle_request, NULL);
    printf("✓ Route registered\n");

    // Listen on port
    printf("\nStarting server on port 8080...\n");
    if (cwh_async_listen(server, 8080) < 0)
    {
        fprintf(stderr, "Failed to listen on port 8080\n");
        cwh_async_server_free(server);
        cwh_loop_free(loop);
        WSACleanup();
        return 1;
    }
    printf("✓ Server listening on port 8080\n\n");

    printf("=========================================\n");
    printf("TEST INSTRUCTIONS:\n");
    printf("1. Open browser to http://localhost:8080/\n");
    printf("2. Or run: curl http://localhost:8080/\n");
    printf("3. Watch for \"Got request\" message below\n");
    printf("4. Press Ctrl+C to stop\n");
    printf("=========================================\n\n");

    printf("Entering event loop (blocking)...\n");
    printf("If AcceptEx works, you'll see connection messages.\n");
    printf("If it hangs here forever, AcceptEx completions aren't delivered.\n\n");

    // Run server (blocks)
    cwh_loop_run(loop);

    // Cleanup
    printf("\nShutting down...\n");
    cwh_async_server_free(server);
    cwh_loop_free(loop);
    WSACleanup();

    printf("Test complete\n");
    return 0;
}
