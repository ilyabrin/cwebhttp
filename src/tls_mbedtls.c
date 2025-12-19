#include "cwebhttp_tls.h"

#if CWEBHTTP_ENABLE_TLS

#include <string.h>
#include <stdlib.h>

#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/ssl_cache.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/x509.h"

// TLS context (global state)
struct cwh_tls_context
{
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_x509_crt cacert;
    mbedtls_x509_crt client_cert;
    mbedtls_pk_context client_key;
    mbedtls_ssl_cache_context cache;
    cwh_tls_config_t config;
    bool has_cacert;
    bool has_client_cert;
    bool has_cache;
};

// TLS session (per-connection)
struct cwh_tls_session
{
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    cwh_tls_context_t *ctx;
    int socket_fd;
    char *hostname;
    bool is_server;
    char sni_hostname[256];
    char client_cert_subject[512];
    bool client_cert_verified;
};

// Error string mapping
const char *cwh_tls_error_string(cwh_tls_error_t error)
{
    switch (error)
    {
    case CWH_TLS_OK:
        return "Success";
    case CWH_TLS_ERR_INIT:
        return "TLS initialization failed";
    case CWH_TLS_ERR_HANDSHAKE:
        return "TLS handshake failed";
    case CWH_TLS_ERR_CERT:
        return "Certificate verification failed";
    case CWH_TLS_ERR_READ:
        return "TLS read error";
    case CWH_TLS_ERR_WRITE:
        return "TLS write error";
    case CWH_TLS_ERR_ALLOC:
        return "Memory allocation failed";
    case CWH_TLS_ERR_INVALID:
        return "Invalid parameter";
    default:
        return "Unknown error";
    }
}

// Check if TLS is available
bool cwh_tls_is_available(void)
{
    return true;
}

// Create TLS context
cwh_tls_context_t *cwh_tls_context_new(const cwh_tls_config_t *config)
{
    if (!config)
    {
        return NULL;
    }

    cwh_tls_context_t *ctx = calloc(1, sizeof(cwh_tls_context_t));
    if (!ctx)
    {
        return NULL;
    }

    // Copy configuration
    ctx->config = *config;

    // Initialize entropy and RNG
    mbedtls_entropy_init(&ctx->entropy);
    mbedtls_ctr_drbg_init(&ctx->ctr_drbg);

    const char *pers = "cwebhttp_tls";
    int ret = mbedtls_ctr_drbg_seed(&ctx->ctr_drbg, mbedtls_entropy_func, &ctx->entropy,
                                    (const unsigned char *)pers, strlen(pers));
    if (ret != 0)
    {
        free(ctx);
        return NULL;
    }

    // Load CA certificates if specified
    ctx->has_cacert = false;
    if (config->ca_cert_path)
    {
        mbedtls_x509_crt_init(&ctx->cacert);
        ret = mbedtls_x509_crt_parse_file(&ctx->cacert, config->ca_cert_path);
        if (ret == 0)
        {
            ctx->has_cacert = true;
        }
        else
        {
            mbedtls_x509_crt_free(&ctx->cacert);
        }
    }

    // Load client certificate and key if specified
    ctx->has_client_cert = false;
    if (config->client_cert && config->client_key)
    {
        mbedtls_x509_crt_init(&ctx->client_cert);
        mbedtls_pk_init(&ctx->client_key);

        ret = mbedtls_x509_crt_parse_file(&ctx->client_cert, config->client_cert);
        if (ret == 0)
        {
            ret = mbedtls_pk_parse_keyfile(&ctx->client_key, config->client_key, NULL);
            if (ret == 0)
            {
                ctx->has_client_cert = true;
            }
            else
            {
                mbedtls_x509_crt_free(&ctx->client_cert);
                mbedtls_pk_free(&ctx->client_key);
            }
        }
    }

    // Initialize session cache if enabled
    ctx->has_cache = false;
    if (config->session_cache)
    {
        mbedtls_ssl_cache_init(&ctx->cache);
        mbedtls_ssl_cache_set_timeout(&ctx->cache, config->session_timeout);
        ctx->has_cache = true;
    }

    return ctx;
}

// Free TLS context
void cwh_tls_context_free(cwh_tls_context_t *ctx)
{
    if (!ctx)
    {
        return;
    }

    if (ctx->has_cacert)
    {
        mbedtls_x509_crt_free(&ctx->cacert);
    }

    if (ctx->has_client_cert)
    {
        mbedtls_x509_crt_free(&ctx->client_cert);
        mbedtls_pk_free(&ctx->client_key);
    }

    if (ctx->has_cache)
    {
        mbedtls_ssl_cache_free(&ctx->cache);
    }

    mbedtls_ctr_drbg_free(&ctx->ctr_drbg);
    mbedtls_entropy_free(&ctx->entropy);
    free(ctx);
}

// Network send/recv callbacks for mbedTLS
static int tls_net_send(void *ctx, const unsigned char *buf, size_t len)
{
    int fd = *(int *)ctx;
    int ret = send(fd, (const char *)buf, (int)len, 0);
    if (ret < 0)
    {
        return MBEDTLS_ERR_NET_SEND_FAILED;
    }
    return ret;
}

static int tls_net_recv(void *ctx, unsigned char *buf, size_t len)
{
    int fd = *(int *)ctx;
    int ret = recv(fd, (char *)buf, (int)len, 0);
    if (ret < 0)
    {
        return MBEDTLS_ERR_NET_RECV_FAILED;
    }
    if (ret == 0)
    {
        return MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY;
    }
    return ret;
}

// SNI callback for server-side
static int sni_callback(void *param, mbedtls_ssl_context *ssl,
                        const unsigned char *hostname, size_t len)
{
    cwh_tls_session_t *session = (cwh_tls_session_t *)param;
    if (session && hostname && len > 0 && len < sizeof(session->sni_hostname))
    {
        memcpy(session->sni_hostname, hostname, len);
        session->sni_hostname[len] = '\0';
    }
    return 0;
}

// Create TLS session (client mode)
cwh_tls_session_t *cwh_tls_session_new(cwh_tls_context_t *ctx, int socket_fd, const char *hostname)
{
    if (!ctx || socket_fd < 0 || !hostname)
    {
        return NULL;
    }

    cwh_tls_session_t *session = calloc(1, sizeof(cwh_tls_session_t));
    if (!session)
    {
        return NULL;
    }

    session->ctx = ctx;
    session->socket_fd = socket_fd;
    session->hostname = strdup(hostname);
    session->is_server = false;
    session->sni_hostname[0] = '\0';
    session->client_cert_subject[0] = '\0';
    session->client_cert_verified = false;

    if (!session->hostname)
    {
        free(session);
        return NULL;
    }

    // Initialize SSL structures
    mbedtls_ssl_init(&session->ssl);
    mbedtls_ssl_config_init(&session->conf);

    int ret;

    // Setup SSL configuration
    ret = mbedtls_ssl_config_defaults(&session->conf,
                                      MBEDTLS_SSL_IS_CLIENT,
                                      MBEDTLS_SSL_TRANSPORT_STREAM,
                                      MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0)
    {
        goto cleanup;
    }

    // Set RNG
    mbedtls_ssl_conf_rng(&session->conf, mbedtls_ctr_drbg_random, &ctx->ctr_drbg);

    // Set CA certificates
    if (ctx->has_cacert)
    {
        mbedtls_ssl_conf_ca_chain(&session->conf, &ctx->cacert, NULL);
    }

    // Set certificate verification mode
    if (ctx->config.verify_peer)
    {
        mbedtls_ssl_conf_authmode(&session->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    }
    else
    {
        mbedtls_ssl_conf_authmode(&session->conf, MBEDTLS_SSL_VERIFY_NONE);
    }

    // Set client certificate if available
    if (ctx->has_client_cert)
    {
        ret = mbedtls_ssl_conf_own_cert(&session->conf, &ctx->client_cert, &ctx->client_key);
        if (ret != 0)
        {
            goto cleanup;
        }
    }

    // Set minimum TLS version
    int min_version = MBEDTLS_SSL_MINOR_VERSION_3; // TLS 1.2 default
    switch (ctx->config.min_tls_version)
    {
    case 0:
        min_version = MBEDTLS_SSL_MINOR_VERSION_1;
        break; // TLS 1.0
    case 1:
        min_version = MBEDTLS_SSL_MINOR_VERSION_2;
        break; // TLS 1.1
    case 2:
        min_version = MBEDTLS_SSL_MINOR_VERSION_3;
        break; // TLS 1.2
    case 3:
        min_version = MBEDTLS_SSL_MINOR_VERSION_4;
        break; // TLS 1.3
    }
    mbedtls_ssl_conf_min_version(&session->conf, MBEDTLS_SSL_MAJOR_VERSION_3, min_version);

    // Enable session cache for resumption
    if (ctx->has_cache)
    {
        mbedtls_ssl_conf_session_cache(&session->conf, &ctx->cache,
                                       mbedtls_ssl_cache_get,
                                       mbedtls_ssl_cache_set);
    }

    // Setup SSL context
    ret = mbedtls_ssl_setup(&session->ssl, &session->conf);
    if (ret != 0)
    {
        goto cleanup;
    }

    // Set hostname for SNI
    ret = mbedtls_ssl_set_hostname(&session->ssl, hostname);
    if (ret != 0)
    {
        goto cleanup;
    }

    // Set I/O callbacks
    mbedtls_ssl_set_bio(&session->ssl, &session->socket_fd, tls_net_send, tls_net_recv, NULL);

    return session;

cleanup:
    mbedtls_ssl_config_free(&session->conf);
    mbedtls_ssl_free(&session->ssl);
    free(session->hostname);
    free(session);
    return NULL;
}

// Create TLS session (server mode)
cwh_tls_session_t *cwh_tls_session_new_server(cwh_tls_context_t *ctx, int socket_fd)
{
    if (!ctx || socket_fd < 0 || !ctx->has_client_cert)
    {
        return NULL;
    }

    cwh_tls_session_t *session = calloc(1, sizeof(cwh_tls_session_t));
    if (!session)
    {
        return NULL;
    }

    session->ctx = ctx;
    session->socket_fd = socket_fd;
    session->hostname = NULL;
    session->is_server = true;
    session->sni_hostname[0] = '\0';
    session->client_cert_subject[0] = '\0';
    session->client_cert_verified = false;

    // Initialize SSL structures
    mbedtls_ssl_init(&session->ssl);
    mbedtls_ssl_config_init(&session->conf);

    int ret;

    // Setup SSL configuration for server
    ret = mbedtls_ssl_config_defaults(&session->conf,
                                      MBEDTLS_SSL_IS_SERVER,
                                      MBEDTLS_SSL_TRANSPORT_STREAM,
                                      MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0)
    {
        goto cleanup;
    }

    // Set RNG
    mbedtls_ssl_conf_rng(&session->conf, mbedtls_ctr_drbg_random, &ctx->ctr_drbg);

    // Set server certificate and key
    ret = mbedtls_ssl_conf_own_cert(&session->conf, &ctx->client_cert, &ctx->client_key);
    if (ret != 0)
    {
        goto cleanup;
    }

    // Set client certificate verification mode
    if (ctx->config.require_client_cert)
    {
        mbedtls_ssl_conf_authmode(&session->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
        if (ctx->has_cacert)
        {
            mbedtls_ssl_conf_ca_chain(&session->conf, &ctx->cacert, NULL);
        }
    }
    else
    {
        mbedtls_ssl_conf_authmode(&session->conf, MBEDTLS_SSL_VERIFY_NONE);
    }

    // Set minimum TLS version
    int min_version = MBEDTLS_SSL_MINOR_VERSION_3;
    switch (ctx->config.min_tls_version)
    {
    case 0:
        min_version = MBEDTLS_SSL_MINOR_VERSION_1;
        break;
    case 1:
        min_version = MBEDTLS_SSL_MINOR_VERSION_2;
        break;
    case 2:
        min_version = MBEDTLS_SSL_MINOR_VERSION_3;
        break;
    case 3:
        min_version = MBEDTLS_SSL_MINOR_VERSION_4;
        break;
    }
    mbedtls_ssl_conf_min_version(&session->conf, MBEDTLS_SSL_MAJOR_VERSION_3, min_version);

    // Enable session cache for resumption
    if (ctx->has_cache)
    {
        mbedtls_ssl_conf_session_cache(&session->conf, &ctx->cache,
                                       mbedtls_ssl_cache_get,
                                       mbedtls_ssl_cache_set);
    }

    // Setup SSL context
    ret = mbedtls_ssl_setup(&session->ssl, &session->conf);
    if (ret != 0)
    {
        goto cleanup;
    }

    // Set SNI callback
    mbedtls_ssl_conf_sni(&session->conf, sni_callback, session);

    // Set I/O callbacks
    mbedtls_ssl_set_bio(&session->ssl, &session->socket_fd, tls_net_send, tls_net_recv, NULL);

    return session;

cleanup:
    mbedtls_ssl_config_free(&session->conf);
    mbedtls_ssl_free(&session->ssl);
    free(session);
    return NULL;
}

// Perform TLS handshake
cwh_tls_error_t cwh_tls_handshake(cwh_tls_session_t *session)
{
    if (!session)
    {
        return CWH_TLS_ERR_INVALID;
    }

    int ret;
    while ((ret = mbedtls_ssl_handshake(&session->ssl)) != 0)
    {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE)
        {
            if (ret == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED)
            {
                return CWH_TLS_ERR_CERT;
            }
            return CWH_TLS_ERR_HANDSHAKE;
        }
    }

    // Verify certificate (if required)
    if (session->is_server && session->ctx->config.require_client_cert)
    {
        const mbedtls_x509_crt *peer_cert = mbedtls_ssl_get_peer_cert(&session->ssl);
        if (peer_cert)
        {
            uint32_t flags = mbedtls_ssl_get_verify_result(&session->ssl);
            if (flags == 0)
            {
                session->client_cert_verified = true;
                mbedtls_x509_dn_gets(session->client_cert_subject,
                                     sizeof(session->client_cert_subject),
                                     &peer_cert->subject);
            }
            else
            {
                return CWH_TLS_ERR_CERT;
            }
        }
        else
        {
            return CWH_TLS_ERR_CERT;
        }
    }
    else if (!session->is_server && session->ctx->config.verify_peer)
    {
        uint32_t flags = mbedtls_ssl_get_verify_result(&session->ssl);
        if (flags != 0)
        {
            return CWH_TLS_ERR_CERT;
        }
    }

    return CWH_TLS_OK;
}

// Read data from TLS connection
int cwh_tls_read(cwh_tls_session_t *session, void *buf, size_t len)
{
    if (!session || !buf)
    {
        return -1;
    }

    int ret = mbedtls_ssl_read(&session->ssl, (unsigned char *)buf, len);
    if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE)
    {
        return 0; // Would block
    }
    if (ret < 0)
    {
        return -1;
    }
    return ret;
}

// Write data to TLS connection
int cwh_tls_write(cwh_tls_session_t *session, const void *buf, size_t len)
{
    if (!session || !buf)
    {
        return -1;
    }

    int ret = mbedtls_ssl_write(&session->ssl, (const unsigned char *)buf, len);
    if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE)
    {
        return 0; // Would block
    }
    if (ret < 0)
    {
        return -1;
    }
    return ret;
}

// Get SNI hostname (server-side)
const char *cwh_tls_get_sni_hostname(cwh_tls_session_t *session)
{
    if (!session || !session->is_server)
    {
        return NULL;
    }
    return session->sni_hostname[0] ? session->sni_hostname : NULL;
}

// Check if client certificate was verified (server-side)
bool cwh_tls_client_cert_verified(cwh_tls_session_t *session)
{
    if (!session || !session->is_server)
    {
        return false;
    }
    return session->client_cert_verified;
}

// Get client certificate subject (server-side)
const char *cwh_tls_get_client_cert_subject(cwh_tls_session_t *session)
{
    if (!session || !session->is_server || !session->client_cert_verified)
    {
        return NULL;
    }
    return session->client_cert_subject[0] ? session->client_cert_subject : NULL;
}

// Free TLS session
void cwh_tls_session_free(cwh_tls_session_t *session)
{
    if (!session)
    {
        return;
    }

    mbedtls_ssl_close_notify(&session->ssl);
    mbedtls_ssl_free(&session->ssl);
    mbedtls_ssl_config_free(&session->conf);
    free(session->hostname);
    free(session);
}

#else // !CWEBHTTP_ENABLE_TLS

// Stub implementations when TLS is disabled
const char *cwh_tls_error_string(cwh_tls_error_t error)
{
    return "TLS not compiled in";
}

bool cwh_tls_is_available(void)
{
    return false;
}

cwh_tls_context_t *cwh_tls_context_new(const cwh_tls_config_t *config)
{
    (void)config;
    return NULL;
}

void cwh_tls_context_free(cwh_tls_context_t *ctx)
{
    (void)ctx;
}

cwh_tls_session_t *cwh_tls_session_new(cwh_tls_context_t *ctx, int socket_fd, const char *hostname)
{
    (void)ctx;
    (void)socket_fd;
    (void)hostname;
    return NULL;
}

cwh_tls_session_t *cwh_tls_session_new_server(cwh_tls_context_t *ctx, int socket_fd)
{
    (void)ctx;
    (void)socket_fd;
    return NULL;
}

cwh_tls_error_t cwh_tls_handshake(cwh_tls_session_t *session)
{
    (void)session;
    return CWH_TLS_ERR_INIT;
}

int cwh_tls_read(cwh_tls_session_t *session, void *buf, size_t len)
{
    (void)session;
    (void)buf;
    (void)len;
    return -1;
}

int cwh_tls_write(cwh_tls_session_t *session, const void *buf, size_t len)
{
    (void)session;
    (void)buf;
    (void)len;
    return -1;
}

const char *cwh_tls_get_sni_hostname(cwh_tls_session_t *session)
{
    (void)session;
    return NULL;
}

bool cwh_tls_client_cert_verified(cwh_tls_session_t *session)
{
    (void)session;
    return false;
}

const char *cwh_tls_get_client_cert_subject(cwh_tls_session_t *session)
{
    (void)session;
    return NULL;
}

void cwh_tls_session_free(cwh_tls_session_t *session)
{
    (void)session;
}

#endif // CWEBHTTP_ENABLE_TLS
