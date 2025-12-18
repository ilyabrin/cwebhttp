// async_server.c - Example demonstrating async HTTP server
// High-performance event-driven server with C10K capability

#include "../include/cwebhttp_async.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#endif
#else
#include <signal.h>
#endif

// Global server for signal handling
static cwh_async_server_t *g_server = NULL;

// ============================================================================
// Route Handlers
// ============================================================================

// Home page handler
void handle_index(cwh_async_conn_t *conn, cwh_request_t *req, void *data)
{
    (void)req;
    (void)data;

    const char *html =
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head><title>Async Server</title></head>\n"
        "<body>\n"
        "<h1>cwebhttp Async Server</h1>\n"
        "<p>Welcome to the async HTTP server!</p>\n"
        "<ul>\n"
        "<li><a href=\"/\">Home</a></li>\n"
        "<li><a href=\"/api/hello\">API Hello</a></li>\n"
        "<li><a href=\"/api/users\">API Users</a></li>\n"
        "</ul>\n"
        "</body>\n"
        "</html>";

    cwh_async_send_response(conn, 200, "text/html", html, strlen(html));
}

// API hello endpoint
void handle_api_hello(cwh_async_conn_t *conn, cwh_request_t *req, void *data)
{
    (void)req;
    (void)data;

    const char *json = "{\"message\":\"Hello from async server!\",\"status\":\"ok\"}";
    cwh_async_send_json(conn, 200, json);
}

// API users endpoint
void handle_api_users(cwh_async_conn_t *conn, cwh_request_t *req, void *data)
{
    (void)req;
    (void)data;

    const char *json =
        "{\n"
        "  \"users\": [\n"
        "    {\"id\": 1, \"name\": \"Alice\", \"email\": \"alice@example.com\"},\n"
        "    {\"id\": 2, \"name\": \"Bob\", \"email\": \"bob@example.com\"},\n"
        "    {\"id\": 3, \"name\": \"Charlie\", \"email\": \"charlie@example.com\"}\n"
        "  ]\n"
        "}";

    cwh_async_send_json(conn, 200, json);
}

// API post handler (echo the request body)
void handle_api_post(cwh_async_conn_t *conn, cwh_request_t *req, void *data)
{
    (void)data;

    char response[1024];
    int len = snprintf(response, sizeof(response),
                       "{\n"
                       "  \"received\": true,\n"
                       "  \"method\": \"%s\",\n"
                       "  \"path\": \"%s\",\n"
                       "  \"body_length\": %lu\n"
                       "}",
                       req->method_str,
                       req->path,
                       (unsigned long)req->body_len);

    if (len > 0)
    {
        cwh_async_send_json(conn, 200, response);
    }
    else
    {
        cwh_async_send_status(conn, 500, "Internal Server Error");
    }
}

// ============================================================================
// Signal Handling
// ============================================================================

#ifndef _WIN32
void signal_handler(int sig)
{
    (void)sig;
    printf("\n\nShutting down server...\n");
    if (g_server)
    {
        cwh_async_server_stop(g_server);
    }
}
#endif

// ============================================================================
// Main
// ============================================================================

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
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
#endif

    // Parse port from command line
    int port = 8080;
    if (argc > 1)
    {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535)
        {
            fprintf(stderr, "Invalid port: %s\n", argv[1]);
            return 1;
        }
    }

    printf("cwebhttp Async Server\n");
    printf("=====================\n\n");

    // Create event loop
    cwh_loop_t *loop = cwh_loop_new();
    if (!loop)
    {
        fprintf(stderr, "Failed to create event loop\n");
        return 1;
    }

    printf("Event loop backend: %s\n\n", cwh_loop_backend(loop));

    // Create async server
    cwh_async_server_t *server = cwh_async_server_new(loop);
    if (!server)
    {
        fprintf(stderr, "Failed to create server\n");
        cwh_loop_free(loop);
        return 1;
    }

    g_server = server;

    // Register routes
    printf("Registering routes:\n");
    printf("  GET  /\n");
    printf("  GET  /api/hello\n");
    printf("  GET  /api/users\n");
    printf("  POST /api/echo\n");
    printf("\n");

    cwh_async_route(server, "GET", "/", handle_index, NULL);
    cwh_async_route(server, "GET", "/api/hello", handle_api_hello, NULL);
    cwh_async_route(server, "GET", "/api/users", handle_api_users, NULL);
    cwh_async_route(server, "POST", "/api/echo", handle_api_post, NULL);

    // Start listening
    printf("Starting server on http://localhost:%d\n", port);
    if (cwh_async_listen(server, port) < 0)
    {
        fprintf(stderr, "Failed to listen on port %d\n", port);
        cwh_async_server_free(server);
        cwh_loop_free(loop);
        return 1;
    }

    printf("Server listening... Press Ctrl+C to stop\n\n");

    // Run event loop
    cwh_loop_run(loop);

    // Cleanup
    printf("\nCleaning up...\n");
    cwh_async_server_free(server);
    cwh_loop_free(loop);

#ifdef _WIN32
    WSACleanup();
#endif

    printf("Server stopped.\n");
    return 0;
}
