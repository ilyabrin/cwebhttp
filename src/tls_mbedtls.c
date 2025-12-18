#include "cwebhttp_tls.h"

#if CWEBHTTP_ENABLE_TLS

#include <string.h>
#include <stdlib.h>

#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/certs.h"
#include "mbedtls/x509.h"

// TLS context (global state)
struct cwh_tls_context
{
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_x509_crt cacert;
    mbedtls_x509_crt client_cert;
    mbedtls_pk_context client_key;
    cwh_tls_config_t config;
    bool has_cacert;
    bool has_client_cert;
};

// TLS session (per-connection)
struct cwh_tls_session
{
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    cwh_tls_context_t *ctx;
    int socket_fd;
    char *hostname;
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

// Create TLS session
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
    if (session->ctx->config.verify_peer)
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

void cwh_tls_session_free(cwh_tls_session_t *session)
{
    (void)session;
}

#endif // CWEBHTTP_ENABLE_TLS
