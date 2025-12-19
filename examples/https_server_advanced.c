// examples/https_server_advanced.c - Advanced HTTPS Server Example
// Demonstrates SNI support, session resumption, and client certificate authentication

#include "../include/cwebhttp_async.h"
#include "../include/cwebhttp.h"
#include "../include/cwebhttp_tls.h"
#include <stdio.h>
#include <string.h>

// Handle root endpoint
void handle_root(cwh_async_conn_t *conn, cwh_request_t *req, void *data)
{
    (void)req;
    (void)data;

    const char *html =
        "<!DOCTYPE html>\n"
        "<html><head><title>Advanced HTTPS Server</title></head>\n"
        "<body>\n"
        "<h1>🔒 Advanced HTTPS Server Features</h1>\n"
        "<h2>Enabled Features:</h2>\n"
        "<ul>\n"
        "<li>✅ SNI (Server Name Indication)</li>\n"
        "<li>✅ TLS Session Resumption</li>\n"
        "<li>✅ Client Certificate Authentication</li>\n"
        "</ul>\n"
        "<p><a href='/api'>API Endpoint</a></p>\n"
        "</body></html>\n";

    cwh_async_send_response(conn, 200, "text/html", html, strlen(html));
}

// Handle API endpoint - shows TLS session info
void handle_api(cwh_async_conn_t *conn, cwh_request_t *req, void *data)
{
    (void)req;
    (void)data;

    char json[1024];
    snprintf(json, sizeof(json),
             "{\n"
             "  \"status\": \"ok\",\n"
             "  \"secure\": true,\n"
             "  \"protocol\": \"https\",\n"
             "  \"features\": {\n"
             "    \"sni\": true,\n"
             "    \"session_resumption\": true,\n"
             "    \"client_cert_auth\": true\n"
             "  }\n"
             "}");

    cwh_async_send_json(conn, 200, json);
}

void print_usage(const char *prog)
{
    printf("Usage: %s <cert.pem> <key.pem> [ca_cert.pem] [port]\n", prog);
    printf("\nArguments:\n");
    printf("  cert.pem       - Server certificate (required)\n");
    printf("  key.pem        - Server private key (required)\n");
    printf("  ca_cert.pem    - CA certificate for client verification (optional)\n");
    printf("  port           - Port number (default: 8443)\n");
    printf("\nExamples:\n");
    printf("  # Basic HTTPS (no client cert):\n");
    printf("  %s server.crt server.key 8443\n\n", prog);
    printf("  # With client certificate authentication:\n");
    printf("  %s server.crt server.key ca.crt 8443\n\n", prog);
    printf("To generate certificates:\n");
    printf("  # Server certificate:\n");
    printf("  openssl req -x509 -newkey rsa:2048 -nodes -keyout server.key -out server.crt -days 365\n\n");
    printf("  # CA certificate:\n");
    printf("  openssl req -x509 -newkey rsa:2048 -nodes -keyout ca.key -out ca.crt -days 365\n\n");
    printf("  # Client certificate signed by CA:\n");
    printf("  openssl genrsa -out client.key 2048\n");
    printf("  openssl req -new -key client.key -out client.csr\n");
    printf("  openssl x509 -req -in client.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out client.crt -days 365\n");
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        print_usage(argv[0]);
        return 1;
    }

    const char *cert_file = argv[1];
    const char *key_file = argv[2];
    const char *ca_cert_file = NULL;
    int port = 8443;
    bool require_client_cert = false;

    // Parse optional arguments
    if (argc >= 4)
    {
        // Check if arg is a number (port) or file (CA cert)
        if (argv[3][0] >= '0' && argv[3][0] <= '9')
        {
            port = atoi(argv[3]);
        }
        else
        {
            ca_cert_file = argv[3];
            require_client_cert = true;
            if (argc >= 5)
            {
                port = atoi(argv[4]);
            }
        }
    }

    printf("========================================\n");
    printf("  Advanced HTTPS Server Example\n");
    printf("========================================\n");
    printf("Server Certificate: %s\n", cert_file);
    printf("Private Key:        %s\n", key_file);
    if (ca_cert_file)
    {
        printf("CA Certificate:     %s\n", ca_cert_file);
        printf("Client Auth:        REQUIRED\n");
    }
    else
    {
        printf("Client Auth:        DISABLED\n");
    }
    printf("Port:               %d\n", port);
    printf("========================================\n\n");

    printf("TLS Features:\n");
    printf("  ✅ SNI Support - Server Name Indication\n");
    printf("  ✅ Session Resumption - Fast reconnects\n");
    if (require_client_cert)
    {
        printf("  ✅ Client Certificate Authentication\n");
    }
    printf("\n");

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

    // Configure TLS with advanced options
    printf("Configuring TLS...\n");
    if (cwh_async_server_set_tls_ex(server, cert_file, key_file, ca_cert_file, require_client_cert) != 0)
    {
        fprintf(stderr, "Failed to configure TLS\n");
        fprintf(stderr, "Make sure:\n");
        fprintf(stderr, "  1. Certificate and key files exist and are valid\n");
        fprintf(stderr, "  2. Files are in PEM format\n");
        fprintf(stderr, "  3. Library compiled with CWEBHTTP_ENABLE_TLS=1\n");
        if (ca_cert_file)
        {
            fprintf(stderr, "  4. CA certificate file exists and is valid\n");
        }
        cwh_async_server_free(server);
        cwh_loop_free(loop);
        return 1;
    }
    printf("✓ TLS configured successfully\n");
    printf("  • SNI enabled (Server Name Indication)\n");
    printf("  • Session cache enabled (resumption support)\n");
    if (require_client_cert)
    {
        printf("  • Client certificate verification enabled\n");
    }
    printf("\n");

    // Register routes
    cwh_async_route(server, "GET", "/", handle_root, NULL);
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

    if (require_client_cert)
    {
        printf("Testing with client certificate:\n");
        printf("  curl --cert client.crt --key client.key --cacert %s https://localhost:%d/\n\n", cert_file, port);
    }
    else
    {
        printf("Testing:\n");
        printf("  curl --insecure https://localhost:%d/\n\n", port);
    }

    printf("Press Ctrl+C to stop\n");
    printf("========================================\n\n");

    // Run event loop
    cwh_loop_run(loop);

    // Cleanup
    cwh_async_server_free(server);
    cwh_loop_free(loop);

    return 0;
}
