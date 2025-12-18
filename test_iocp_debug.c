// test_iocp_debug.c - Debug version with detailed logging
#include "include/cwebhttp_async.h"
#include "include/cwebhttp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#endif
#else
#error "This test is Windows-only"
#endif

// Simple request handler with debug output
void handle_request(cwh_async_conn_t *conn, cwh_request_t *req, void *data)
{
    (void)data;
    printf("===========================================\n");
    printf("[DEBUG] Handler called!\n");
    printf("[DEBUG] Method: %s\n", req->method_str);
    printf("[DEBUG] Path: %s\n", req->path);
    printf("[DEBUG] Connection: %p\n", (void *)conn);
    printf("===========================================\n");

    const char *body = "Hello, IOCP!\n";

    printf("[DEBUG] Sending response: 200 OK, body length: %zu\n", strlen(body));
    cwh_async_send_response(conn, 200, "text/plain", body, strlen(body));
    printf("[DEBUG] Response sent!\n");
}

int main(void)
{
    printf("========================================\n");
    printf("IOCP Async Server DEBUG Test\n");
    printf("========================================\n\n");

    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
    {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }
    printf("[DEBUG] Winsock initialized\n\n");

    // Create event loop
    printf("[1] Creating event loop...\n");
    cwh_loop_t *loop = cwh_loop_new();
    if (!loop)
    {
        fprintf(stderr, "Failed to create event loop\n");
        WSACleanup();
        return 1;
    }
    printf("✓ Event loop created\n");
    printf("  Backend: %s\n\n", cwh_loop_backend(loop));

    // Verify IOCP
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
    printf("[2] Creating async server...\n");
    cwh_async_server_t *server = cwh_async_server_new(loop);
    if (!server)
    {
        fprintf(stderr, "Failed to create server\n");
        cwh_loop_free(loop);
        WSACleanup();
        return 1;
    }
    printf("✓ Server created\n\n");

    // Add route
    printf("[3] Registering route...\n");
    cwh_async_route(server, "GET", "/", handle_request, NULL);
    printf("✓ Route registered: GET /\n\n");

    // Listen
    printf("[4] Starting to listen on port 8080...\n");
    if (cwh_async_listen(server, 8080) < 0)
    {
        fprintf(stderr, "Failed to listen on port 8080\n");
        fprintf(stderr, "Error: %d\n", WSAGetLastError());
        cwh_async_server_free(server);
        cwh_loop_free(loop);
        WSACleanup();
        return 1;
    }
    printf("✓ Server listening on port 8080\n\n");

    printf("========================================\n");
    printf("SERVER READY\n");
    printf("========================================\n");
    printf("Test with:\n");
    printf("  curl http://localhost:8080/\n");
    printf("  or open in browser\n\n");
    printf("Watch for [DEBUG] messages below\n");
    printf("Press Ctrl+C to stop\n");
    printf("========================================\n\n");

    printf("[5] Entering event loop...\n\n");

    // Run server
    cwh_loop_run(loop);

    // Cleanup
    printf("\n[CLEANUP] Shutting down...\n");
    cwh_async_server_free(server);
    cwh_loop_free(loop);
    WSACleanup();

    printf("Test complete\n");
    return 0;
}
