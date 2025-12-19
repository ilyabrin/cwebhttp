#ifndef CWEBHTTP_ERROR_H
#define CWEBHTTP_ERROR_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // Enhanced error codes with detailed categories
    typedef enum
    {
        // Success
        CWH_OK = 0,

        // Parse errors (100-199)
        CWH_ERR_PARSE_INVALID_REQUEST = -100,
        CWH_ERR_PARSE_INVALID_METHOD = -101,
        CWH_ERR_PARSE_INVALID_URL = -102,
        CWH_ERR_PARSE_INVALID_HEADER = -103,
        CWH_ERR_PARSE_BODY_TOO_LARGE = -104,
        CWH_ERR_PARSE_INCOMPLETE = -105,

        // Network errors (200-299)
        CWH_ERR_NET_SOCKET_CREATE = -200,
        CWH_ERR_NET_SOCKET_BIND = -201,
        CWH_ERR_NET_SOCKET_LISTEN = -202,
        CWH_ERR_NET_SOCKET_ACCEPT = -203,
        CWH_ERR_NET_SOCKET_CONNECT = -204,
        CWH_ERR_NET_SEND = -205,
        CWH_ERR_NET_RECV = -206,
        CWH_ERR_NET_TIMEOUT = -207,
        CWH_ERR_NET_CONNECTION_CLOSED = -208,
        CWH_ERR_NET_DNS_LOOKUP = -209,
        CWH_ERR_NET_INVALID_ADDRESS = -210,

        // Memory errors (300-399)
        CWH_ERR_ALLOC_FAILED = -300,
        CWH_ERR_ALLOC_BUFFER_TOO_SMALL = -301,
        CWH_ERR_ALLOC_OUT_OF_MEMORY = -302,

        // File errors (400-499)
        CWH_ERR_FILE_NOT_FOUND = -400,
        CWH_ERR_FILE_ACCESS_DENIED = -401,
        CWH_ERR_FILE_READ = -402,
        CWH_ERR_FILE_WRITE = -403,
        CWH_ERR_FILE_TOO_LARGE = -404,

        // Server errors (500-599)
        CWH_ERR_SERVER_INIT = -500,
        CWH_ERR_SERVER_START = -501,
        CWH_ERR_SERVER_ROUTE_EXISTS = -502,
        CWH_ERR_SERVER_NO_HANDLER = -503,
        CWH_ERR_SERVER_MAX_CONNECTIONS = -504,

        // Client errors (600-699)
        CWH_ERR_CLIENT_INIT = -600,
        CWH_ERR_CLIENT_REQUEST_BUILD = -601,
        CWH_ERR_CLIENT_RESPONSE_PARSE = -602,
        CWH_ERR_CLIENT_REDIRECT_LIMIT = -603,

        // Event loop errors (700-799)
        CWH_ERR_LOOP_INIT = -700,
        CWH_ERR_LOOP_ADD_FD = -701,
        CWH_ERR_LOOP_MOD_FD = -702,
        CWH_ERR_LOOP_DEL_FD = -703,
        CWH_ERR_LOOP_WAIT = -704,
        CWH_ERR_LOOP_BACKEND_NOT_SUPPORTED = -705,

        // SSL/TLS errors (800-899)
        CWH_ERR_SSL_INIT = -800,
        CWH_ERR_SSL_HANDSHAKE = -801,
        CWH_ERR_SSL_CERT_VERIFY = -802,

        // Generic errors (900-999)
        CWH_ERR_INVALID_ARGUMENT = -900,
        CWH_ERR_NOT_IMPLEMENTED = -901,
        CWH_ERR_INTERNAL = -902,

        // Legacy compatibility
        CWH_ERR_PARSE = -1,
        CWH_ERR_NET = -2,
        CWH_ERR_ALLOC = -3,
        CWH_ERR_TIMEOUT = -4
    } cwh_error_code_t;

    // Error context structure
    typedef struct
    {
        cwh_error_code_t code;
        const char *message;
        const char *file;
        int line;
        const char *function;
        int system_errno;  // System errno if applicable
        char details[256]; // Additional details
    } cwh_error_t;

    // Error handling functions
    const char *cwh_error_string(cwh_error_code_t code);
    const char *cwh_error_category(cwh_error_code_t code);
    void cwh_error_init(cwh_error_t *error);
    void cwh_error_set(cwh_error_t *error, cwh_error_code_t code, const char *message, const char *file, int line, const char *func);
    void cwh_error_set_details(cwh_error_t *error, const char *fmt, ...);
    void cwh_error_print(const cwh_error_t *error);
    void cwh_error_clear(cwh_error_t *error);

// Macro helpers for error setting with location info
#define CWH_ERROR_SET(err, code, msg) \
    cwh_error_set((err), (code), (msg), __FILE__, __LINE__, __func__)

#define CWH_ERROR_RETURN(err, code, msg)                                   \
    do                                                                     \
    {                                                                      \
        cwh_error_set((err), (code), (msg), __FILE__, __LINE__, __func__); \
        return (code);                                                     \
    } while (0)

    // Thread-local error storage (optional, for APIs that don't take error param)
    cwh_error_t *cwh_get_last_error(void);
    void cwh_set_last_error(cwh_error_code_t code, const char *message);

#ifdef __cplusplus
}
#endif

#endif // CWEBHTTP_ERROR_H
