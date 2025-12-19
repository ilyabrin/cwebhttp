#include "cwebhttp_error.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

// Error code to string mapping
const char *cwh_error_string(cwh_error_code_t code)
{
    switch (code)
    {
    case CWH_OK:
        return "Success";

    // Parse errors
    case CWH_ERR_PARSE_INVALID_REQUEST:
        return "Invalid HTTP request format";
    case CWH_ERR_PARSE_INVALID_METHOD:
        return "Invalid or unsupported HTTP method";
    case CWH_ERR_PARSE_INVALID_URL:
        return "Invalid URL format";
    case CWH_ERR_PARSE_INVALID_HEADER:
        return "Invalid HTTP header format";
    case CWH_ERR_PARSE_BODY_TOO_LARGE:
        return "Request body exceeds maximum size";
    case CWH_ERR_PARSE_INCOMPLETE:
        return "Incomplete HTTP message";

    // Network errors
    case CWH_ERR_NET_SOCKET_CREATE:
        return "Failed to create socket";
    case CWH_ERR_NET_SOCKET_BIND:
        return "Failed to bind socket to address";
    case CWH_ERR_NET_SOCKET_LISTEN:
        return "Failed to listen on socket";
    case CWH_ERR_NET_SOCKET_ACCEPT:
        return "Failed to accept connection";
    case CWH_ERR_NET_SOCKET_CONNECT:
        return "Failed to connect to remote host";
    case CWH_ERR_NET_SEND:
        return "Failed to send data";
    case CWH_ERR_NET_RECV:
        return "Failed to receive data";
    case CWH_ERR_NET_TIMEOUT:
        return "Network operation timed out";
    case CWH_ERR_NET_CONNECTION_CLOSED:
        return "Connection closed by peer";
    case CWH_ERR_NET_DNS_LOOKUP:
        return "DNS lookup failed";
    case CWH_ERR_NET_INVALID_ADDRESS:
        return "Invalid network address";

    // Memory errors
    case CWH_ERR_ALLOC_FAILED:
        return "Memory allocation failed";
    case CWH_ERR_ALLOC_BUFFER_TOO_SMALL:
        return "Buffer too small for operation";
    case CWH_ERR_ALLOC_OUT_OF_MEMORY:
        return "Out of memory";

    // File errors
    case CWH_ERR_FILE_NOT_FOUND:
        return "File not found";
    case CWH_ERR_FILE_ACCESS_DENIED:
        return "File access denied";
    case CWH_ERR_FILE_READ:
        return "Failed to read file";
    case CWH_ERR_FILE_WRITE:
        return "Failed to write file";
    case CWH_ERR_FILE_TOO_LARGE:
        return "File too large";

    // Server errors
    case CWH_ERR_SERVER_INIT:
        return "Failed to initialize server";
    case CWH_ERR_SERVER_START:
        return "Failed to start server";
    case CWH_ERR_SERVER_ROUTE_EXISTS:
        return "Route already exists";
    case CWH_ERR_SERVER_NO_HANDLER:
        return "No handler found for route";
    case CWH_ERR_SERVER_MAX_CONNECTIONS:
        return "Maximum connections reached";

    // Client errors
    case CWH_ERR_CLIENT_INIT:
        return "Failed to initialize client";
    case CWH_ERR_CLIENT_REQUEST_BUILD:
        return "Failed to build HTTP request";
    case CWH_ERR_CLIENT_RESPONSE_PARSE:
        return "Failed to parse HTTP response";
    case CWH_ERR_CLIENT_REDIRECT_LIMIT:
        return "Too many redirects";

    // Event loop errors
    case CWH_ERR_LOOP_INIT:
        return "Failed to initialize event loop";
    case CWH_ERR_LOOP_ADD_FD:
        return "Failed to add file descriptor to event loop";
    case CWH_ERR_LOOP_MOD_FD:
        return "Failed to modify file descriptor in event loop";
    case CWH_ERR_LOOP_DEL_FD:
        return "Failed to remove file descriptor from event loop";
    case CWH_ERR_LOOP_WAIT:
        return "Event loop wait failed";
    case CWH_ERR_LOOP_BACKEND_NOT_SUPPORTED:
        return "Event loop backend not supported on this platform";

    // SSL/TLS errors
    case CWH_ERR_SSL_INIT:
        return "Failed to initialize SSL/TLS";
    case CWH_ERR_SSL_HANDSHAKE:
        return "SSL/TLS handshake failed";
    case CWH_ERR_SSL_CERT_VERIFY:
        return "SSL/TLS certificate verification failed";

    // Generic errors
    case CWH_ERR_INVALID_ARGUMENT:
        return "Invalid argument";
    case CWH_ERR_NOT_IMPLEMENTED:
        return "Feature not implemented";
    case CWH_ERR_INTERNAL:
        return "Internal error";

    // Legacy
    case CWH_ERR_PARSE:
        return "Parse error";
    case CWH_ERR_NET:
        return "Network error";
    case CWH_ERR_ALLOC:
        return "Allocation error";
    case CWH_ERR_TIMEOUT:
        return "Timeout error";

    default:
        return "Unknown error";
    }
}

// Get error category
const char *cwh_error_category(cwh_error_code_t code)
{
    int abs_code = code < 0 ? -code : code;

    if (abs_code >= 100 && abs_code < 200)
        return "Parse";
    if (abs_code >= 200 && abs_code < 300)
        return "Network";
    if (abs_code >= 300 && abs_code < 400)
        return "Memory";
    if (abs_code >= 400 && abs_code < 500)
        return "File";
    if (abs_code >= 500 && abs_code < 600)
        return "Server";
    if (abs_code >= 600 && abs_code < 700)
        return "Client";
    if (abs_code >= 700 && abs_code < 800)
        return "EventLoop";
    if (abs_code >= 800 && abs_code < 900)
        return "SSL";
    if (abs_code >= 900 && abs_code < 1000)
        return "Generic";

    return "Unknown";
}

// Initialize error structure
void cwh_error_init(cwh_error_t *error)
{
    if (!error)
        return;

    error->code = CWH_OK;
    error->message = NULL;
    error->file = NULL;
    error->line = 0;
    error->function = NULL;
    error->system_errno = 0;
    error->details[0] = '\0';
}

// Set error with context
void cwh_error_set(cwh_error_t *error, cwh_error_code_t code, const char *message,
                   const char *file, int line, const char *func)
{
    if (!error)
        return;

    error->code = code;
    error->message = message ? message : cwh_error_string(code);
    error->file = file;
    error->line = line;
    error->function = func;
    error->system_errno = errno;
    error->details[0] = '\0';
}

// Set additional error details
void cwh_error_set_details(cwh_error_t *error, const char *fmt, ...)
{
    if (!error || !fmt)
        return;

    va_list args;
    va_start(args, fmt);
    vsnprintf(error->details, sizeof(error->details), fmt, args);
    va_end(args);
}

// Print error information
void cwh_error_print(const cwh_error_t *error)
{
    if (!error || error->code == CWH_OK)
        return;

    fprintf(stderr, "[ERROR] %s (%d): %s\n",
            cwh_error_category(error->code), error->code, error->message);

    if (error->details[0] != '\0')
    {
        fprintf(stderr, "  Details: %s\n", error->details);
    }

    if (error->file && error->line > 0)
    {
        fprintf(stderr, "  Location: %s:%d", error->file, error->line);
        if (error->function)
        {
            fprintf(stderr, " in %s()", error->function);
        }
        fprintf(stderr, "\n");
    }

    if (error->system_errno != 0)
    {
        fprintf(stderr, "  System error: %s (%d)\n",
                strerror(error->system_errno), error->system_errno);
    }
}

// Clear error
void cwh_error_clear(cwh_error_t *error)
{
    if (!error)
        return;
    cwh_error_init(error);
}

// Thread-local error storage
#ifdef _WIN32
__declspec(thread) static cwh_error_t tls_error = {0};
#else
static __thread cwh_error_t tls_error = {0};
#endif

cwh_error_t *cwh_get_last_error(void)
{
    return &tls_error;
}

void cwh_set_last_error(cwh_error_code_t code, const char *message)
{
    tls_error.code = code;
    tls_error.message = message ? message : cwh_error_string(code);
    tls_error.system_errno = errno;
}
