#include "cwebhttp.h"
#include "cwebhttp_tls.h"
#include "cwebhttp_log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#endif

// Simple HTTPS GET request example
int main(int argc, char *argv[])
{
    const char *hostname = "www.example.com";
    const char *port = "443";
    const char *path = "/";

    if (argc > 1)
    {
        hostname = argv[1];
    }
    if (argc > 2)
    {
        path = argv[2];
    }

    // Initialize logging
    cwh_log_set_level(CWH_LOG_INFO);

#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    cwh_log_info("Connecting to %s:%s", hostname, port);

    // Resolve hostname
    struct addrinfo hints = {0};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *result = NULL;
    int ret = getaddrinfo(hostname, port, &hints, &result);
    if (ret != 0)
    {
        cwh_log_error("DNS resolution failed: %d", ret);
        return 1;
    }

    // Create socket
    int sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sock < 0)
    {
        cwh_log_error("Socket creation failed");
        freeaddrinfo(result);
        return 1;
    }

    // Connect
    if (connect(sock, result->ai_addr, (int)result->ai_addrlen) < 0)
    {
        cwh_log_error("Connection failed");
        freeaddrinfo(result);
#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
        return 1;
    }

    freeaddrinfo(result);
    cwh_log_info("TCP connection established");

    // Check if TLS is available
    if (!cwh_tls_is_available())
    {
        cwh_log_error("TLS support not compiled in. Rebuild with CWEBHTTP_ENABLE_TLS=1");
#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
        return 1;
    }

    // Initialize TLS context
    cwh_tls_config_t tls_config = cwh_tls_config_default();
    tls_config.verify_peer = false; // Disable for this example (not recommended for production!)

    cwh_tls_context_t *tls_ctx = cwh_tls_context_new(&tls_config);
    if (!tls_ctx)
    {
        cwh_log_error("Failed to create TLS context");
#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
        return 1;
    }

    cwh_log_info("TLS context created");

    // Create TLS session
    cwh_tls_session_t *tls_session = cwh_tls_session_new(tls_ctx, sock, hostname);
    if (!tls_session)
    {
        cwh_log_error("Failed to create TLS session");
        cwh_tls_context_free(tls_ctx);
#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
        return 1;
    }

    cwh_log_info("Performing TLS handshake...");

    // Perform TLS handshake
    cwh_tls_error_t tls_err = cwh_tls_handshake(tls_session);
    if (tls_err != CWH_TLS_OK)
    {
        cwh_log_error("TLS handshake failed: %s", cwh_tls_error_string(tls_err));
        cwh_tls_session_free(tls_session);
        cwh_tls_context_free(tls_ctx);
#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
        return 1;
    }

    cwh_log_info("TLS handshake successful!");

    // Build HTTP request
    char request[1024];
    snprintf(request, sizeof(request),
             "GET %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "User-Agent: cwebhttp/0.8.0\r\n"
             "Accept: */*\r\n"
             "Connection: close\r\n"
             "\r\n",
             path, hostname);

    cwh_log_info("Sending request:\n%s", request);

    // Send request over TLS
    int sent = cwh_tls_write(tls_session, request, strlen(request));
    if (sent < 0)
    {
        cwh_log_error("Failed to send request");
        cwh_tls_session_free(tls_session);
        cwh_tls_context_free(tls_ctx);
#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
        return 1;
    }

    cwh_log_info("Request sent (%d bytes)", sent);

    // Receive response
    char buffer[4096];
    int total_received = 0;

    printf("\n=== Response ===\n");
    while (1)
    {
        int received = cwh_tls_read(tls_session, buffer, sizeof(buffer) - 1);
        if (received <= 0)
        {
            break;
        }
        buffer[received] = '\0';
        printf("%s", buffer);
        total_received += received;
    }

    cwh_log_info("\nTotal received: %d bytes", total_received);

    // Cleanup
    cwh_tls_session_free(tls_session);
    cwh_tls_context_free(tls_ctx);

#ifdef _WIN32
    closesocket(sock);
    WSACleanup();
#else
    close(sock);
#endif

    return 0;
}
