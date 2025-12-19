// examples/https_server.c - Simple HTTPS Server Example
// Demonstrates async HTTPS server with TLS/SSL support

#include "../include/cwebhttp_async.h"
#include "../include/cwebhttp.h"
#include <stdio.h>
#include <string.h>

// Simple GET handler
void handle_get(cwh_async_conn_t *conn, cwh_request_t *req, void *data)
{
    (void)req;
    (void)data;

    const char *html =
        "<!DOCTYPE html>\n"
        "<html><head><title>HTTPS Server</title></head>\n"
        "<body>\n"
        "<h1>🔒 Secure HTTPS Server</h1>\n"
        "<p>This page is served over HTTPS!</p>\n"
        "<p>TLS encryption is active.</p>\n"
        "</body></html>\n";

    cwh_async_send_response(conn, 200, "text/html", html, strlen(html));
}

// JSON API handler
void handle_api(cwh_async_conn_t *conn, cwh_request_t *req, void *data)
{
    (void)req;
    (void)data;

    const char *json = "{\"status\":\"ok\",\"secure\":true,\"protocol\":\"https\"}";
    cwh_async_send_json(conn, 200, json);
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        printf("Usage: %s <cert.pem> <key.pem> [port]\n", argv[0]);
        printf("\nExample:\n");
        printf("  %s server.crt server.key 8443\n", argv[0]);
        printf("\nTo generate self-signed certificate:\n");
        printf("  openssl req -x509 -newkey rsa:2048 -nodes -keyout server.key -out server.crt -days 365\n");
        return 1;
    }

    const char *cert_file = argv[1];
    const char *key_file = argv[2];
    int port = (argc > 3) ? atoi(argv[3]) : 8443;

    printf("========================================\n");
    printf("     HTTPS Server Example\n");
    printf("========================================\n");
    printf("Certificate: %s\n", cert_file);
    printf("Private Key: %s\n", key_file);
    printf("Port:        %d\n", port);
    printf("========================================\n\n");

    // Create event loop
    cwh_loop_t *loop = cwh_loop_new();
    if (!loop)
    {
        fprintf(stderr, "Failed to create event loop\n");
        return 1;
    }

    printf("Event loop backend: %s\n", cwh_loop_backend(loop));

    // Create async server
    cwh_async_server_t *server = cwh_async_server_new(loop);
    if (!server)
    {
        fprintf(stderr, "Failed to create server\n");
        cwh_loop_free(loop);
        return 1;
    }

    // Configure TLS/HTTPS
    printf("Configuring TLS...\n");
    if (cwh_async_server_set_tls(server, cert_file, key_file) != 0)
    {
        fprintf(stderr, "Failed to configure TLS\n");
        fprintf(stderr, "Make sure:\n");
        fprintf(stderr, "  1. Certificate and key files exist\n");
        fprintf(stderr, "  2. Files are in PEM format\n");
        fprintf(stderr, "  3. Library compiled with CWEBHTTP_ENABLE_TLS=1\n");
        cwh_async_server_free(server);
        cwh_loop_free(loop);
        return 1;
    }
    printf("✓ TLS configured\n\n");

    // Register routes
    cwh_async_route(server, "GET", "/", handle_get, NULL);
    cwh_async_route(server, "GET", "/api", handle_api, NULL);

    // Start listening
    printf("Starting HTTPS server on port %d...\n", port);
    if (cwh_async_listen(server, port) != 0)
    {
        fprintf(stderr, "Failed to start server\n");
        cwh_async_server_free(server);
        cwh_loop_free(loop);
        return 1;
    }

    printf("✓ Server listening on https://localhost:%d\n\n", port);
    printf("Available endpoints:\n");
    printf("  https://localhost:%d/      - HTML page\n", port);
    printf("  https://localhost:%d/api   - JSON API\n\n", port);
    printf("Press Ctrl+C to stop\n");
    printf("========================================\n\n");

    // Run event loop
    cwh_loop_run(loop);

    // Cleanup
    cwh_async_server_free(server);
    cwh_loop_free(loop);

    return 0;
}
