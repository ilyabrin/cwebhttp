#ifndef CWEBHTTP_H
#define CWEBHTTP_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h> // для malloc/free в примерах, но zero-alloc в core
#include <time.h>   // for time_t in cookie expiration

// Версия
#define CWEBHTTP_VERSION "0.1.0"

// Ошибки
typedef enum
{
    CWH_OK = 0,
    CWH_ERR_PARSE = -1,
    CWH_ERR_NET = -2,
    CWH_ERR_ALLOC = -3,
    CWH_ERR_TIMEOUT = -4
} cwh_error_t;

// HTTP-методы (строки для простоты)
typedef enum
{
    CWH_METHOD_GET,
    CWH_METHOD_POST,
    CWH_METHOD_PUT,
    CWH_METHOD_DELETE,
    CWH_METHOD_NUM // для массива
} cwh_method_t;

extern const char *cwh_method_strs[CWH_METHOD_NUM + 1];

// Структура запроса (zero-alloc: указатели в буфер)
typedef struct
{
    char *method_str; // "GET" и т.д.
    char *path;
    char *query;       // "?key=val"
    char *headers[32]; // key-value пары, NULL-terminated (headers[i*2] = key, [i*2+1] = val)
    size_t num_headers;
    char *body;
    size_t body_len;
    bool is_valid;
} cwh_request_t;

// Структура ответа
typedef struct
{
    int status;        // 200, 404 и т.д.
    char *headers[32]; // как в req
    size_t num_headers;
    char *body;
    size_t body_len;
} cwh_response_t;

// URL structure (zero-alloc: указатели в буфер)
typedef struct
{
    char *scheme;   // "http" or "https"
    char *host;     // "example.com" or "192.168.1.1"
    char *port_str; // "8080" or NULL (use default)
    int port;       // 80, 443, or parsed custom port
    char *path;     // "/api/users" or "/" (default)
    char *query;    // "page=1&limit=10" or NULL
    char *fragment; // "section" or NULL (after #)
    bool is_valid;
} cwh_url_t;

// Forward declare TLS types (defined in cwebhttp_tls.h)
struct cwh_tls_context;
struct cwh_tls_session;

// Абстракция соединения с keep-alive поддержкой
typedef struct cwh_conn
{
    int fd;
    char *host;
    int port;
    bool keep_alive;                     // Connection supports keep-alive
    time_t last_used;                    // Timestamp of last use (for timeout)
    bool is_https;                       // Whether this is an HTTPS connection
    struct cwh_tls_context *tls_ctx;     // TLS context (if HTTPS)
    struct cwh_tls_session *tls_session; // TLS session (if HTTPS)
    struct cwh_conn *next;               // For connection pool linked list
} cwh_conn_t;

// Cookie structure for cookie jar
typedef struct cwh_cookie
{
    char *name;              // Cookie name (allocated)
    char *value;             // Cookie value (allocated)
    char *domain;            // Domain (allocated, e.g., ".example.com")
    char *path;              // Path (allocated, e.g., "/")
    time_t expires;          // Expiration time (0 = session cookie)
    bool secure;             // Secure flag (HTTPS only)
    bool http_only;          // HttpOnly flag (no JavaScript access)
    struct cwh_cookie *next; // Linked list
} cwh_cookie_t;

// Клиент API
cwh_conn_t *cwh_connect(const char *url, int timeout_ms);
cwh_error_t cwh_send_req(cwh_conn_t *conn, cwh_method_t method, const char *path, const char **headers, const char *body, size_t body_len);
cwh_error_t cwh_read_res(cwh_conn_t *conn, cwh_response_t *res);
void cwh_close(cwh_conn_t *conn);

// Connection pool API (for keep-alive support)
void cwh_pool_init(void);                             // Initialize connection pool
void cwh_pool_cleanup(void);                          // Cleanup all pooled connections
void cwh_pool_return(cwh_conn_t *conn);               // Return connection to pool or close it
cwh_conn_t *cwh_pool_get(const char *host, int port); // Get connection from pool

// Cookie jar API (for automatic cookie management)
void cwh_cookie_jar_init(void);                                             // Initialize cookie jar
void cwh_cookie_jar_cleanup(void);                                          // Free all cookies
void cwh_cookie_jar_add(const char *domain, const char *set_cookie_header); // Add cookie from Set-Cookie header
char *cwh_cookie_jar_get(const char *domain, const char *path);             // Get cookies for domain/path (returns allocated string)

// Сервер API (пока sync, async потом)
typedef cwh_error_t (*cwh_handler_t)(cwh_request_t *req, cwh_conn_t *conn, void *user_data);
typedef struct cwh_server cwh_server_t;
cwh_server_t *cwh_listen(const char *addr_port, int backlog);
cwh_error_t cwh_route(cwh_server_t *srv, const char *method, const char *pattern, cwh_handler_t handler, void *user_data);
cwh_error_t cwh_run(cwh_server_t *srv); // blocking event loop
void cwh_free_server(cwh_server_t *srv);

// Server response helpers
cwh_error_t cwh_send_response(cwh_conn_t *conn, int status, const char *content_type,
                              const char *body, size_t body_len);
cwh_error_t cwh_send_status(cwh_conn_t *conn, int status, const char *message);

// Static file serving
const char *cwh_get_mime_type(const char *path);
cwh_error_t cwh_send_file(cwh_conn_t *conn, const char *file_path);
cwh_error_t cwh_send_file_range(cwh_conn_t *conn, const char *file_path, const char *range_header);
cwh_error_t cwh_serve_static(cwh_request_t *req, cwh_conn_t *conn, void *root_dir);

// Парсинг (zero-alloc)
cwh_error_t cwh_parse_req(const char *buf, size_t len, cwh_request_t *req);
cwh_error_t cwh_parse_res(const char *buf, size_t len, cwh_response_t *res);
cwh_error_t cwh_format_res(char *buf, size_t *out_len, const cwh_response_t *res);
cwh_error_t cwh_format_req(char *buf, size_t *out_len, const cwh_request_t *req);

// Utility functions
const char *cwh_get_header(const cwh_request_t *req, const char *key);
const char *cwh_get_res_header(const cwh_response_t *res, const char *key);

// URL parsing (zero-alloc)
cwh_error_t cwh_parse_url(const char *url, size_t len, cwh_url_t *parsed);

// Chunked transfer encoding (RFC 7230 Section 4.1)
cwh_error_t cwh_decode_chunked(const char *chunked_body, size_t chunked_len, char *out_buf, size_t *out_len);
cwh_error_t cwh_encode_chunked(const char *body, size_t body_len, char *out_buf, size_t *out_len);

// Response decompression (gzip/deflate)
cwh_error_t cwh_decompress_gzip(const char *compressed, size_t compressed_len, char *out_buf, size_t *out_len);
cwh_error_t cwh_decompress_deflate(const char *compressed, size_t compressed_len, char *out_buf, size_t *out_len);

// High-level convenience API (one-liners for simple requests)
cwh_error_t cwh_get(const char *url, cwh_response_t *res);
cwh_error_t cwh_post(const char *url, const char *body, size_t body_len, cwh_response_t *res);
cwh_error_t cwh_put(const char *url, const char *body, size_t body_len, cwh_response_t *res);
cwh_error_t cwh_delete(const char *url, cwh_response_t *res);

// Route entry (internal)
typedef struct cwh_route
{
    char *method;           // "GET", "POST", etc. (NULL = any method)
    char *pattern;          // "/api/users", "/", etc.
    cwh_handler_t handler;  // Request handler function
    void *user_data;        // User data passed to handler
    struct cwh_route *next; // Linked list
} cwh_route_t;

// Внутренние (не для юзера)
struct cwh_server
{
    int sock;            // Server socket
    cwh_route_t *routes; // Linked list of routes
};

#endif // CWEBHTTP_H