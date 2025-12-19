#ifndef CWEBHTTP_TLS_H
#define CWEBHTTP_TLS_H

#include <stddef.h>
#include <stdbool.h>

// TLS feature flag (compile-time)
#ifndef CWEBHTTP_ENABLE_TLS
#define CWEBHTTP_ENABLE_TLS 0
#endif

// Forward declarations
typedef struct cwh_tls_context cwh_tls_context_t;
typedef struct cwh_tls_session cwh_tls_session_t;

// TLS error codes
typedef enum
{
    CWH_TLS_OK = 0,
    CWH_TLS_ERR_INIT = -1,
    CWH_TLS_ERR_HANDSHAKE = -2,
    CWH_TLS_ERR_CERT = -3,
    CWH_TLS_ERR_READ = -4,
    CWH_TLS_ERR_WRITE = -5,
    CWH_TLS_ERR_ALLOC = -6,
    CWH_TLS_ERR_INVALID = -7
} cwh_tls_error_t;

// TLS configuration
typedef struct
{
    bool verify_peer;         // Verify server certificate (default: true)
    const char *ca_cert_path; // Path to CA certificate file (NULL = system default)
    const char *ca_cert_dir;  // Path to CA certificate directory
    const char *client_cert;  // Client certificate (optional)
    const char *client_key;   // Client private key (optional)
    const char *ciphers;      // Cipher suite configuration (NULL = default)
    int min_tls_version;      // Minimum TLS version (0=TLS1.0, 1=TLS1.1, 2=TLS1.2, 3=TLS1.3)
    int timeout_ms;           // Handshake timeout in milliseconds

    // Server-side options
    bool require_client_cert; // Require client certificate authentication (server)
    bool session_cache;       // Enable TLS session resumption cache
    int session_timeout;      // Session cache timeout in seconds (default: 86400)
} cwh_tls_config_t;

// TLS context API (global initialization)
cwh_tls_context_t *cwh_tls_context_new(const cwh_tls_config_t *config);
void cwh_tls_context_free(cwh_tls_context_t *ctx);

// TLS session API (per-connection)
cwh_tls_session_t *cwh_tls_session_new(cwh_tls_context_t *ctx, int socket_fd, const char *hostname);
cwh_tls_session_t *cwh_tls_session_new_server(cwh_tls_context_t *ctx, int socket_fd);
cwh_tls_error_t cwh_tls_handshake(cwh_tls_session_t *session);
int cwh_tls_read(cwh_tls_session_t *session, void *buf, size_t len);
int cwh_tls_write(cwh_tls_session_t *session, const void *buf, size_t len);
void cwh_tls_session_free(cwh_tls_session_t *session);

// Server-side TLS utilities
const char *cwh_tls_get_sni_hostname(cwh_tls_session_t *session);
bool cwh_tls_client_cert_verified(cwh_tls_session_t *session);
const char *cwh_tls_get_client_cert_subject(cwh_tls_session_t *session);

// Utility functions
const char *cwh_tls_error_string(cwh_tls_error_t error);
bool cwh_tls_is_available(void);

// Default configuration
static inline cwh_tls_config_t cwh_tls_config_default(void)
{
    cwh_tls_config_t config = {
        .verify_peer = true,
        .ca_cert_path = NULL,
        .ca_cert_dir = NULL,
        .client_cert = NULL,
        .client_key = NULL,
        .ciphers = NULL,
        .min_tls_version = 2, // TLS 1.2 minimum
        .timeout_ms = 5000,
        .require_client_cert = false,
        .session_cache = true,
        .session_timeout = 86400}; // 24 hours
    return config;
}

#endif // CWEBHTTP_TLS_H
