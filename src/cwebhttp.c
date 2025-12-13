#if !defined(_WIN32) && !defined(_WIN64)
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif

#include "cwebhttp.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include <zlib.h>

#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <ws2tcpip.h>
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#else
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <strings.h>
#include <fcntl.h>
#include <netdb.h>
#include <errno.h>
#endif

// Definition of cwh_method_strs
const char *cwh_method_strs[CWH_METHOD_NUM + 1] = {
    "GET", "POST", "PUT", "DELETE", NULL};

// Platform-specific socket initialization
#if defined(_WIN32) || defined(_WIN64)
static int init_winsock(void)
{
    static int winsock_initialized = 0;
    if (!winsock_initialized)
    {
        WSADATA wsa_data;
        if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
            return -1;
        winsock_initialized = 1;
    }
    return 0;
}

#define CLOSE_SOCKET(fd) closesocket(fd)
#define SOCKET_ERROR_CODE WSAGetLastError()
#else
#define CLOSE_SOCKET(fd) close(fd)
#define SOCKET_ERROR_CODE errno
#endif

// Set socket to non-blocking mode
static int set_nonblocking(int fd)
{
#if defined(_WIN32) || defined(_WIN64)
    u_long mode = 1;
    return ioctlsocket(fd, FIONBIO, &mode);
#else
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#endif
}

// ============================================================================
// Connection Pool for Keep-Alive Support
// ============================================================================

#define CWH_POOL_MAX_IDLE_TIME 60  // Close connections idle for 60 seconds
#define CWH_POOL_MAX_CONNECTIONS 10 // Maximum pooled connections

static cwh_conn_t *g_connection_pool = NULL; // Head of connection pool linked list
static int g_pool_size = 0;                  // Current number of pooled connections

// Initialize connection pool (called automatically on first use)
void cwh_pool_init(void)
{
    g_connection_pool = NULL;
    g_pool_size = 0;
}

// Get a connection from the pool for the given host:port
// Returns NULL if no matching connection is available
cwh_conn_t *cwh_pool_get(const char *host, int port)
{
    if (!host)
        return NULL;

    cwh_conn_t **prev_ptr = &g_connection_pool;
    cwh_conn_t *curr = g_connection_pool;
    time_t now = time(NULL);

    while (curr)
    {
        // Check if this connection matches host:port and is still valid
        if (curr->host && strcmp(curr->host, host) == 0 && curr->port == port)
        {
            // Check if connection is too old (idle timeout)
            if (now - curr->last_used > CWH_POOL_MAX_IDLE_TIME)
            {
                // Connection expired - remove from pool and close
                *prev_ptr = curr->next;
                CLOSE_SOCKET(curr->fd);
                free(curr->host);
                free(curr);
                g_pool_size--;
                curr = *prev_ptr;
                continue;
            }

            // Found a valid connection - remove from pool and return it
            *prev_ptr = curr->next;
            curr->next = NULL;
            curr->last_used = now;
            g_pool_size--;
            return curr;
        }

        prev_ptr = &curr->next;
        curr = curr->next;
    }

    return NULL; // No matching connection found
}

// Return a connection to the pool (or close it if not keep-alive)
void cwh_pool_return(cwh_conn_t *conn)
{
    if (!conn)
        return;

    // If connection doesn't support keep-alive, close it immediately
    if (!conn->keep_alive)
    {
        if (conn->fd >= 0)
            CLOSE_SOCKET(conn->fd);
        free(conn->host);
        free(conn);
        return;
    }

    // If pool is full, close oldest connection
    if (g_pool_size >= CWH_POOL_MAX_CONNECTIONS)
    {
        // Close the connection instead of adding to pool
        if (conn->fd >= 0)
            CLOSE_SOCKET(conn->fd);
        free(conn->host);
        free(conn);
        return;
    }

    // Add connection to pool
    conn->last_used = time(NULL);
    conn->next = g_connection_pool;
    g_connection_pool = conn;
    g_pool_size++;
}

// Cleanup all pooled connections (call at program exit)
void cwh_pool_cleanup(void)
{
    cwh_conn_t *curr = g_connection_pool;
    while (curr)
    {
        cwh_conn_t *next = curr->next;
        if (curr->fd >= 0)
            CLOSE_SOCKET(curr->fd);
        free(curr->host);
        free(curr);
        curr = next;
    }
    g_connection_pool = NULL;
    g_pool_size = 0;
}

// ============================================================================
// End Connection Pool
// ============================================================================

// Connect to host with timeout
cwh_conn_t *cwh_connect(const char *url, int timeout_ms)
{
    if (!url)
        return NULL;

#if defined(_WIN32) || defined(_WIN64)
    if (init_winsock() != 0)
        return NULL;
#endif

    // Parse URL
    cwh_url_t parsed = {0};
    if (cwh_parse_url(url, strlen(url), &parsed) != CWH_OK)
        return NULL;

    if (!parsed.is_valid || !parsed.host)
        return NULL;

    // Convert host and port to null-terminated strings
    char host[256] = {0};
    char port_str[16] = {0};

    // Extract host (find length until ':' or end)
    const char *host_end = parsed.host;
    while (*host_end && *host_end != ':' && *host_end != '/' &&
           *host_end != '?' && *host_end != '#')
        host_end++;

    size_t host_len = host_end - parsed.host;
    if (host_len >= sizeof(host))
        return NULL;

    memcpy(host, parsed.host, host_len);
    host[host_len] = '\0';

    snprintf(port_str, sizeof(port_str), "%d", parsed.port);

    // Try to get an existing connection from the pool
    cwh_conn_t *conn = cwh_pool_get(host, parsed.port);
    if (conn)
    {
        // Found a pooled connection - reuse it
        return conn;
    }

    // No pooled connection available - create a new one

    // Resolve hostname using getaddrinfo
    struct addrinfo hints = {0};
    hints.ai_family = AF_UNSPEC;     // IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo *result = NULL;
    if (getaddrinfo(host, port_str, &hints, &result) != 0)
        return NULL;

    // Try each address until we successfully connect
    int sock = -1;
    struct addrinfo *rp;
    for (rp = result; rp != NULL; rp = rp->ai_next)
    {
        sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock < 0)
            continue;

        // Set non-blocking for timeout support
        if (timeout_ms > 0)
        {
            if (set_nonblocking(sock) != 0)
            {
                CLOSE_SOCKET(sock);
                sock = -1;
                continue;
            }
        }

        // Attempt connection
        int conn_result = connect(sock, rp->ai_addr, (int)rp->ai_addrlen);

        if (conn_result == 0)
        {
            // Connected immediately
            break;
        }

#if defined(_WIN32) || defined(_WIN64)
        int err = WSAGetLastError();
        if (timeout_ms > 0 && err == WSAEWOULDBLOCK)
#else
        if (timeout_ms > 0 && (errno == EINPROGRESS || errno == EWOULDBLOCK))
#endif
        {
            // Wait for connection with timeout
            fd_set write_fds;
            FD_ZERO(&write_fds);
            FD_SET(sock, &write_fds);

            struct timeval tv;
            tv.tv_sec = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000;

            int sel_result = select(sock + 1, NULL, &write_fds, NULL, &tv);

            if (sel_result > 0)
            {
                // Check if connection succeeded
                int sock_err = 0;
                socklen_t len = sizeof(sock_err);
                if (getsockopt(sock, SOL_SOCKET, SO_ERROR, (char *)&sock_err, &len) == 0 && sock_err == 0)
                {
                    break; // Connected successfully
                }
            }
        }

        CLOSE_SOCKET(sock);
        sock = -1;
    }

    freeaddrinfo(result);

    if (sock < 0)
        return NULL;

    // Create connection object
    conn = malloc(sizeof(cwh_conn_t));
    if (!conn)
    {
        CLOSE_SOCKET(sock);
        return NULL;
    }

    conn->fd = sock;
    conn->host = strdup(host);
    conn->port = parsed.port;
    conn->keep_alive = false;    // Will be set to true if server supports it
    conn->last_used = time(NULL);
    conn->next = NULL;

    if (!conn->host)
    {
        CLOSE_SOCKET(sock);
        free(conn);
        return NULL;
    }

    return conn;
}

// Send data with timeout
static int send_with_timeout(int fd, const char *buf, size_t len, int timeout_ms)
{
    size_t sent = 0;

    while (sent < len)
    {
        // Wait for socket to be writable
        fd_set write_fds;
        FD_ZERO(&write_fds);
        FD_SET(fd, &write_fds);

        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;

        int sel_result = select(fd + 1, NULL, &write_fds, NULL, &tv);

        if (sel_result < 0)
            return -1; // Error
        if (sel_result == 0)
            return -2; // Timeout

        // Send data
        int n = send(fd, buf + sent, (int)(len - sent), 0);
        if (n < 0)
        {
#if defined(_WIN32) || defined(_WIN64)
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK)
                continue;
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                continue;
#endif
            return -1; // Error
        }

        sent += n;
    }

    return (int)sent;
}

// Receive data with timeout
static int recv_with_timeout(int fd, char *buf, size_t len, int timeout_ms)
{
    // Wait for socket to be readable
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int sel_result = select(fd + 1, &read_fds, NULL, NULL, &tv);

    if (sel_result < 0)
        return -1; // Error
    if (sel_result == 0)
        return -2; // Timeout

    // Receive data
    int n = recv(fd, buf, (int)len, 0);
    return n;
}

cwh_error_t cwh_send_req(cwh_conn_t *conn, cwh_method_t method, const char *path, const char **headers, const char *body, size_t body_len)
{
    if (!conn || conn->fd < 0 || !path)
        return CWH_ERR_NET;

    // Build request
    char req_buf[8192] = {0};
    size_t offset = 0;

    // Request line
    const char *method_str = cwh_method_strs[method];
    offset += snprintf(req_buf + offset, sizeof(req_buf) - offset,
                       "%s %s HTTP/1.1\r\n", method_str, path);

    // Host header (required for HTTP/1.1)
    offset += snprintf(req_buf + offset, sizeof(req_buf) - offset,
                       "Host: %s\r\n", conn->host);

    // Connection: keep-alive header (enable persistent connections)
    offset += snprintf(req_buf + offset, sizeof(req_buf) - offset,
                       "Connection: keep-alive\r\n");

    // Accept-Encoding header (indicate support for gzip and deflate compression)
    offset += snprintf(req_buf + offset, sizeof(req_buf) - offset,
                       "Accept-Encoding: gzip, deflate\r\n");

    // Additional headers
    if (headers)
    {
        for (int i = 0; headers[i] != NULL && headers[i + 1] != NULL; i += 2)
        {
            offset += snprintf(req_buf + offset, sizeof(req_buf) - offset,
                               "%s: %s\r\n", headers[i], headers[i + 1]);
        }
    }

    // Content-Length if body present
    if (body && body_len > 0)
    {
        offset += snprintf(req_buf + offset, sizeof(req_buf) - offset,
                           "Content-Length: %llu\r\n", (unsigned long long)body_len);
    }

    // End of headers
    offset += snprintf(req_buf + offset, sizeof(req_buf) - offset, "\r\n");

    // Send headers
    if (send_with_timeout(conn->fd, req_buf, offset, 5000) < 0)
        return CWH_ERR_TIMEOUT;

    // Send body if present
    if (body && body_len > 0)
    {
        if (send_with_timeout(conn->fd, body, body_len, 5000) < 0)
            return CWH_ERR_TIMEOUT;
    }

    return CWH_OK;
}

cwh_error_t cwh_read_res(cwh_conn_t *conn, cwh_response_t *res)
{
    if (!conn || conn->fd < 0 || !res)
        return CWH_ERR_NET;

    // Receive response (simplified - just read what's available)
    static char recv_buf[16384];
    int n = recv_with_timeout(conn->fd, recv_buf, sizeof(recv_buf) - 1, 5000);

    if (n < 0)
        return CWH_ERR_TIMEOUT;
    if (n == 0)
        return CWH_ERR_NET; // Connection closed

    recv_buf[n] = '\0';

    // Parse response
    cwh_error_t err = cwh_parse_res(recv_buf, n, res);
    if (err != CWH_OK)
        return err;

    // Check if server supports keep-alive
    const char *connection_hdr = cwh_get_res_header(res, "Connection");
    if (connection_hdr && strcasecmp(connection_hdr, "keep-alive") == 0)
    {
        conn->keep_alive = true;
    }
    else if (connection_hdr && strcasecmp(connection_hdr, "close") == 0)
    {
        conn->keep_alive = false;
    }
    else
    {
        // Default to keep-alive for HTTP/1.1, close for HTTP/1.0
        // We assume HTTP/1.1 unless explicitly stated otherwise
        conn->keep_alive = true;
    }

    return CWH_OK;
}

void cwh_close(cwh_conn_t *conn)
{
    // Delegate to connection pool - it will decide whether to pool or close
    cwh_pool_return(conn);
}

// Helper: skip whitespace
static inline const char *skip_ws(const char *p, const char *end)
{
    while (p < end && (*p == ' ' || *p == '\t'))
        p++;
    return p;
}

// Helper: skip until CRLF
static inline const char *skip_to_crlf(const char *p, const char *end)
{
    while (p + 1 < end && !(p[0] == '\r' && p[1] == '\n'))
        p++;
    return p;
}

// Helper: find character
static inline const char *find_char(const char *p, const char *end, char c)
{
    while (p < end && *p != c)
        p++;
    return p;
}

// Parse HTTP method - zero-alloc, returns pointer into buffer
static cwh_error_t parse_method(const char **p, const char *end, char **method_out, cwh_method_t *method_type)
{
    const char *start = *p;
    const char *method_end = find_char(start, end, ' ');

    if (method_end >= end)
        return CWH_ERR_PARSE;

    size_t method_len = method_end - start;

    // Match method string
    if (method_len == 3 && memcmp(start, "GET", 3) == 0)
    {
        *method_type = CWH_METHOD_GET;
    }
    else if (method_len == 4 && memcmp(start, "POST", 4) == 0)
    {
        *method_type = CWH_METHOD_POST;
    }
    else if (method_len == 3 && memcmp(start, "PUT", 3) == 0)
    {
        *method_type = CWH_METHOD_PUT;
    }
    else if (method_len == 6 && memcmp(start, "DELETE", 6) == 0)
    {
        *method_type = CWH_METHOD_DELETE;
    }
    else if (method_len == 4 && memcmp(start, "HEAD", 4) == 0)
    {
        *method_type = CWH_METHOD_GET; // Treat HEAD as GET for now
    }
    else
    {
        return CWH_ERR_PARSE;
    }

    *method_out = (char *)start;
    *p = method_end;
    return CWH_OK;
}

// Parse path and query - zero-alloc
static cwh_error_t parse_path(const char **p, const char *end, char **path_out, char **query_out)
{
    *p = skip_ws(*p, end);
    const char *start = *p;
    const char *path_end = find_char(start, end, ' ');

    if (path_end >= end)
        return CWH_ERR_PARSE;

    // Find query string separator
    const char *query = find_char(start, path_end, '?');

    if (query < path_end)
    {
        *path_out = (char *)start;
        *query_out = (char *)(query + 1); // Skip '?'
    }
    else
    {
        *path_out = (char *)start;
        *query_out = NULL;
    }

    *p = path_end;
    return CWH_OK;
}

// Parse HTTP version
static cwh_error_t parse_version(const char **p, const char *end)
{
    *p = skip_ws(*p, end);

    if (*p + 8 > end)
        return CWH_ERR_PARSE;

    if (memcmp(*p, "HTTP/1.1", 8) == 0 || memcmp(*p, "HTTP/1.0", 8) == 0)
    {
        *p += 8;
        return CWH_OK;
    }

    return CWH_ERR_PARSE;
}

// Parse single header - zero-alloc
static cwh_error_t parse_header(const char **p, const char *end, char **key_out, char **val_out)
{
    const char *start = *p;

    // Check for end of headers (CRLF)
    if (start + 1 < end && start[0] == '\r' && start[1] == '\n')
    {
        *p = start + 2;
        return CWH_OK; // End of headers
    }

    // Find colon
    const char *colon = find_char(start, end, ':');
    if (colon >= end)
        return CWH_ERR_PARSE;

    *key_out = (char *)start;

    // Skip colon and whitespace
    const char *val_start = skip_ws(colon + 1, end);
    const char *val_end = skip_to_crlf(val_start, end);

    if (val_end + 1 >= end)
        return CWH_ERR_PARSE;

    *val_out = (char *)val_start;
    *p = val_end + 2; // Skip CRLF

    return CWH_OK;
}

// Full HTTP request parser - zero-allocation
cwh_error_t cwh_parse_req(const char *buf, size_t len, cwh_request_t *req)
{
    if (!buf || !req || len == 0)
        return CWH_ERR_PARSE;

    memset(req, 0, sizeof(*req));

    const char *p = buf;
    const char *end = buf + len;

    // Parse request line: METHOD PATH HTTP/VERSION
    cwh_method_t method_type;
    if (parse_method(&p, end, &req->method_str, &method_type) != CWH_OK)
        return CWH_ERR_PARSE;

    if (parse_path(&p, end, &req->path, &req->query) != CWH_OK)
        return CWH_ERR_PARSE;

    if (parse_version(&p, end) != CWH_OK)
        return CWH_ERR_PARSE;

    // Skip CRLF after request line
    if (p + 1 >= end || p[0] != '\r' || p[1] != '\n')
        return CWH_ERR_PARSE;
    p += 2;

    // Parse headers
    req->num_headers = 0;
    while (p < end && req->num_headers < 32)
    {
        char *key = NULL;
        char *val = NULL;

        // Check for end of headers
        if (p + 1 < end && p[0] == '\r' && p[1] == '\n')
        {
            p += 2;
            break;
        }

        if (parse_header(&p, end, &key, &val) != CWH_OK)
            break;

        if (key && val)
        {
            req->headers[req->num_headers * 2] = key;
            req->headers[req->num_headers * 2 + 1] = val;
            req->num_headers++;
        }
    }

    // Body starts after headers
    if (p < end)
    {
        req->body = (char *)p;
        req->body_len = end - p;
    }

    // Null-terminate strings for easier use (modify buffer)
    // This is safe to do after parsing is complete
    if (req->method_str)
    {
        // Find end of method (space after method)
        char *method_end = req->method_str;
        while (*method_end && *method_end != ' ')
            method_end++;
        if (*method_end == ' ')
            *method_end = '\0';
    }

    if (req->path)
    {
        // Find end of path (space, '?', or '\r')
        char *path_end = req->path;
        while (*path_end && *path_end != ' ' && *path_end != '?' && *path_end != '\r')
            path_end++;
        if (*path_end == ' ' || *path_end == '?' || *path_end == '\r')
            *path_end = '\0';
    }

    req->is_valid = true;
    return CWH_OK;
}

// Parse HTTP response status line
static cwh_error_t parse_status_line(const char **p, const char *end, int *status_out)
{
    // Expect "HTTP/1.1 200 OK\r\n"
    if (*p + 8 > end)
        return CWH_ERR_PARSE;

    if (memcmp(*p, "HTTP/1.1", 8) != 0 && memcmp(*p, "HTTP/1.0", 8) != 0)
        return CWH_ERR_PARSE;

    *p += 8;
    *p = skip_ws(*p, end);

    // Parse status code (3 digits)
    if (*p + 3 > end)
        return CWH_ERR_PARSE;

    int status = 0;
    for (int i = 0; i < 3; i++)
    {
        if ((*p)[i] < '0' || (*p)[i] > '9')
            return CWH_ERR_PARSE;
        status = status * 10 + ((*p)[i] - '0');
    }

    *status_out = status;
    *p += 3;

    // Skip reason phrase
    *p = skip_to_crlf(*p, end);
    if (*p + 1 >= end)
        return CWH_ERR_PARSE;

    *p += 2; // Skip CRLF
    return CWH_OK;
}

// Parse HTTP response - zero-allocation
cwh_error_t cwh_parse_res(const char *buf, size_t len, cwh_response_t *res)
{
    if (!buf || !res || len == 0)
        return CWH_ERR_PARSE;

    memset(res, 0, sizeof(*res));

    const char *p = buf;
    const char *end = buf + len;

    // Parse status line
    if (parse_status_line(&p, end, &res->status) != CWH_OK)
        return CWH_ERR_PARSE;

    // Parse headers (same as request)
    res->num_headers = 0;
    while (p < end && res->num_headers < 32)
    {
        char *key = NULL;
        char *val = NULL;

        // Check for end of headers
        if (p + 1 < end && p[0] == '\r' && p[1] == '\n')
        {
            p += 2;
            break;
        }

        if (parse_header(&p, end, &key, &val) != CWH_OK)
            break;

        if (key && val)
        {
            res->headers[res->num_headers * 2] = key;
            res->headers[res->num_headers * 2 + 1] = val;
            res->num_headers++;
        }
    }

    // Body starts after headers
    if (p < end)
    {
        res->body = (char *)p;
        res->body_len = end - p;

        // Check if Transfer-Encoding: chunked
        const char *transfer_encoding = cwh_get_res_header(res, "Transfer-Encoding");
        if (transfer_encoding && strncasecmp(transfer_encoding, "chunked", 7) == 0)
        {
            // Decode chunked body in-place
            // NOTE: This modifies the buffer, but that's acceptable since we own it
            static char decoded_buf[16384]; // Static buffer for decoded data
            size_t decoded_len = 0;

            cwh_error_t err = cwh_decode_chunked(res->body, res->body_len, decoded_buf, &decoded_len);
            if (err == CWH_OK)
            {
                // Update body to point to decoded data
                // IMPORTANT: decoded_buf is static, so this is safe as long as
                // response is used before next cwh_parse_res call
                memcpy((char *)res->body, decoded_buf, decoded_len);
                res->body_len = decoded_len;
            }
            // If decode fails, keep original chunked body (graceful degradation)
        }

        // Check if Content-Encoding is present (gzip/deflate compression)
        const char *content_encoding = cwh_get_res_header(res, "Content-Encoding");
        if (content_encoding)
        {
            static char decompressed_buf[65536]; // Static buffer for decompressed data (64KB)
            size_t decompressed_len = sizeof(decompressed_buf);
            cwh_error_t err = CWH_ERR_PARSE;

            // Try gzip decompression
            if (strncasecmp(content_encoding, "gzip", 4) == 0)
            {
                err = cwh_decompress_gzip(res->body, res->body_len, decompressed_buf, &decompressed_len);
            }
            // Try deflate decompression
            else if (strncasecmp(content_encoding, "deflate", 7) == 0)
            {
                err = cwh_decompress_deflate(res->body, res->body_len, decompressed_buf, &decompressed_len);
            }

            // If decompression succeeded, update body
            if (err == CWH_OK)
            {
                memcpy((char *)res->body, decompressed_buf, decompressed_len);
                res->body_len = decompressed_len;
            }
            // If decompression fails, keep original compressed body (graceful degradation)
        }
    }

    return CWH_OK;
}

// Format HTTP response to buffer
cwh_error_t cwh_format_res(char *buf, size_t *out_len, const cwh_response_t *res)
{
    if (!buf || !out_len || !res)
        return CWH_ERR_PARSE;

    size_t offset = 0;

    // Status line
    offset += snprintf(buf + offset, 1024 - offset, "HTTP/1.1 %d OK\r\n", res->status);

    // Headers
    for (size_t i = 0; i < res->num_headers; i++)
    {
        const char *key = res->headers[i * 2];
        const char *val = res->headers[i * 2 + 1];
        offset += snprintf(buf + offset, 1024 - offset, "%s: %s\r\n", key, val);
    }

    // Content-Length if body present
    if (res->body && res->body_len > 0)
    {
        offset += snprintf(buf + offset, 1024 - offset, "Content-Length: %llu\r\n",
                           (unsigned long long)res->body_len);
    }

    // End of headers
    offset += snprintf(buf + offset, 1024 - offset, "\r\n");

    // Body
    if (res->body && res->body_len > 0)
    {
        memcpy(buf + offset, res->body, res->body_len);
        offset += res->body_len;
    }

    *out_len = offset;
    return CWH_OK;
}

// Format HTTP request to buffer
cwh_error_t cwh_format_req(char *buf, size_t *out_len, const cwh_request_t *req)
{
    if (!buf || !out_len || !req)
        return CWH_ERR_PARSE;

    size_t offset = 0;

    // Request line
    offset += snprintf(buf + offset, 1024 - offset, "%s %s",
                       req->method_str, req->path);

    if (req->query)
    {
        offset += snprintf(buf + offset, 1024 - offset, "?%s", req->query);
    }

    offset += snprintf(buf + offset, 1024 - offset, " HTTP/1.1\r\n");

    // Headers
    for (size_t i = 0; i < req->num_headers; i++)
    {
        const char *key = req->headers[i * 2];
        const char *val = req->headers[i * 2 + 1];
        offset += snprintf(buf + offset, 1024 - offset, "%s: %s\r\n", key, val);
    }

    // Content-Length if body present
    if (req->body && req->body_len > 0)
    {
        offset += snprintf(buf + offset, 1024 - offset, "Content-Length: %llu\r\n",
                           (unsigned long long)req->body_len);
    }

    // End of headers
    offset += snprintf(buf + offset, 1024 - offset, "\r\n");

    // Body
    if (req->body && req->body_len > 0)
    {
        memcpy(buf + offset, req->body, req->body_len);
        offset += req->body_len;
    }

    *out_len = offset;
    return CWH_OK;
}

// Get header value from request (case-insensitive)
const char *cwh_get_header(const cwh_request_t *req, const char *key)
{
    if (!req || !key)
        return NULL;

    size_t key_len = strlen(key);

    for (size_t i = 0; i < req->num_headers; i++)
    {
        const char *hdr_key = req->headers[i * 2];
        const char *hdr_val = req->headers[i * 2 + 1];

        // Case-insensitive compare
        if (strncasecmp(hdr_key, key, key_len) == 0)
            return hdr_val;
    }

    return NULL;
}

// Get header value from response (case-insensitive)
const char *cwh_get_res_header(const cwh_response_t *res, const char *key)
{
    if (!res || !key)
        return NULL;

    size_t key_len = strlen(key);

    for (size_t i = 0; i < res->num_headers; i++)
    {
        const char *hdr_key = res->headers[i * 2];
        const char *hdr_val = res->headers[i * 2 + 1];

        // Case-insensitive compare
        if (strncasecmp(hdr_key, key, key_len) == 0)
            return hdr_val;
    }

    return NULL;
}

// ============================================================================
// URL Parser (zero-alloc)
// ============================================================================

// Parse port string to integer
static int parse_port_str(const char *port_str, size_t len)
{
    if (!port_str || len == 0 || len > 5)
        return -1;

    int port = 0;
    for (size_t i = 0; i < len; i++)
    {
        if (port_str[i] < '0' || port_str[i] > '9')
            return -1;
        port = port * 10 + (port_str[i] - '0');
        if (port > 65535)
            return -1;
    }

    return port > 0 ? port : -1;
}

// URL parser: scheme://host:port/path?query#fragment
// All pointers reference the original buffer (zero-alloc)
cwh_error_t cwh_parse_url(const char *url, size_t len, cwh_url_t *parsed)
{
    if (!url || !parsed || len == 0)
        return CWH_ERR_PARSE;

    memset(parsed, 0, sizeof(*parsed));

    const char *p = url;
    const char *end = url + len;

    // 1. Parse scheme (http:// or https://)
    const char *scheme_end = find_char(p, end, ':');
    if (scheme_end + 2 >= end || scheme_end[1] != '/' || scheme_end[2] != '/')
        return CWH_ERR_PARSE;

    size_t scheme_len = scheme_end - p;
    if (scheme_len == 4 && memcmp(p, "http", 4) == 0)
    {
        parsed->scheme = (char *)p;
        parsed->port = 80; // default HTTP port
    }
    else if (scheme_len == 5 && memcmp(p, "https", 5) == 0)
    {
        parsed->scheme = (char *)p;
        parsed->port = 443; // default HTTPS port
    }
    else
    {
        return CWH_ERR_PARSE; // unsupported scheme
    }

    p = scheme_end + 3; // skip "://"

    // 2. Parse host and optional port
    // Find end of host:port section (before /, ?, or #)
    const char *host_start = p;
    const char *host_end = p;

    while (host_end < end)
    {
        char c = *host_end;
        if (c == '/' || c == '?' || c == '#')
            break;
        host_end++;
    }

    // Check for port separator
    const char *port_sep = find_char(host_start, host_end, ':');

    if (port_sep < host_end)
    {
        // Has explicit port
        parsed->host = (char *)host_start;
        parsed->port_str = (char *)(port_sep + 1);

        // Parse port number
        size_t port_len = host_end - (port_sep + 1);
        int port = parse_port_str(port_sep + 1, port_len);
        if (port < 0)
            return CWH_ERR_PARSE;

        parsed->port = port;
    }
    else
    {
        // No explicit port, use default
        parsed->host = (char *)host_start;
        parsed->port_str = NULL;
    }

    // 3. Parse path, query, fragment
    p = host_end;

    if (p >= end)
    {
        // No path, use default "/"
        parsed->path = NULL; // Will be interpreted as "/"
        parsed->is_valid = true;
        return CWH_OK;
    }

    // Parse path
    if (*p == '/')
    {
        const char *path_start = p;
        const char *path_end = find_char(p, end, '?');

        if (path_end >= end)
            path_end = find_char(p, end, '#');

        if (path_end >= end)
            path_end = end;

        parsed->path = (char *)path_start;
        p = path_end;
    }

    // Parse query
    if (p < end && *p == '?')
    {
        const char *query_start = p + 1;
        const char *query_end = find_char(query_start, end, '#');

        if (query_end >= end)
            query_end = end;

        parsed->query = (char *)query_start;
        p = query_end;
    }

    // Parse fragment
    if (p < end && *p == '#')
    {
        parsed->fragment = (char *)(p + 1);
    }

    parsed->is_valid = true;
    return CWH_OK;
}

// ============================================================================
// Chunked Transfer Encoding (RFC 7230 Section 4.1)
// ============================================================================

// Decode chunked transfer encoding
// Format: <chunk-size-hex>\r\n<chunk-data>\r\n ... 0\r\n\r\n
cwh_error_t cwh_decode_chunked(const char *chunked_body, size_t chunked_len,
                               char *out_buf, size_t *out_len)
{
    if (!chunked_body || !out_buf || !out_len)
        return CWH_ERR_PARSE;

    const char *p = chunked_body;
    const char *end = chunked_body + chunked_len;
    size_t total_decoded = 0;

    while (p < end)
    {
        // Parse chunk size (hexadecimal)
        size_t chunk_size = 0;

        // Read hex digits
        while (p < end && *p != '\r')
        {
            char c = *p;
            if (c >= '0' && c <= '9')
                chunk_size = chunk_size * 16 + (c - '0');
            else if (c >= 'a' && c <= 'f')
                chunk_size = chunk_size * 16 + (c - 'a' + 10);
            else if (c >= 'A' && c <= 'F')
                chunk_size = chunk_size * 16 + (c - 'A' + 10);
            else if (c == ';')
            {
                // Chunk extension - skip until CRLF
                while (p < end && *p != '\r')
                    p++;
                break;
            }
            else
                return CWH_ERR_PARSE; // Invalid hex character

            p++;
        }

        // Expect CRLF after chunk size
        if (p + 1 >= end || p[0] != '\r' || p[1] != '\n')
            return CWH_ERR_PARSE;
        p += 2;

        // Check for last chunk (size 0)
        if (chunk_size == 0)
        {
            // Last chunk - expect optional trailers then final CRLF
            // For simplicity, skip trailers and just look for final CRLF
            if (p + 1 < end && p[0] == '\r' && p[1] == '\n')
                p += 2;
            break;
        }

        // Read chunk data
        if (p + chunk_size > end)
            return CWH_ERR_PARSE; // Chunk size exceeds remaining data

        // Copy chunk data to output
        memcpy(out_buf + total_decoded, p, chunk_size);
        total_decoded += chunk_size;
        p += chunk_size;

        // Expect CRLF after chunk data
        if (p + 1 >= end || p[0] != '\r' || p[1] != '\n')
            return CWH_ERR_PARSE;
        p += 2;
    }

    *out_len = total_decoded;
    return CWH_OK;
}

// Encode data as chunked transfer encoding
// Format: <chunk-size-hex>\r\n<chunk-data>\r\n ... 0\r\n\r\n
cwh_error_t cwh_encode_chunked(const char *body, size_t body_len,
                               char *out_buf, size_t *out_len)
{
    if (!body || !out_buf || !out_len)
        return CWH_ERR_PARSE;

    size_t offset = 0;
    const size_t chunk_size = 4096; // Standard chunk size

    // Encode body in chunks
    size_t remaining = body_len;
    const char *p = body;

    while (remaining > 0)
    {
        size_t current_chunk = remaining < chunk_size ? remaining : chunk_size;

        // Write chunk size in hex
        // Use %lx for portability (cast size_t to unsigned long)
        int hex_len = snprintf(out_buf + offset, 32, "%lx\r\n", (unsigned long)current_chunk);
        if (hex_len < 0)
            return CWH_ERR_PARSE;
        offset += hex_len;

        // Write chunk data
        memcpy(out_buf + offset, p, current_chunk);
        offset += current_chunk;
        p += current_chunk;
        remaining -= current_chunk;

        // Write CRLF after chunk data
        out_buf[offset++] = '\r';
        out_buf[offset++] = '\n';
    }

    // Write final chunk (0 size)
    memcpy(out_buf + offset, "0\r\n\r\n", 5);
    offset += 5;

    *out_len = offset;
    return CWH_OK;
}

// ============================================================================
// Response Decompression (gzip/deflate)
// ============================================================================

// Decompress gzip-compressed data
// gzip uses zlib with gzip header (window bits = 15 + 16)
cwh_error_t cwh_decompress_gzip(const char *compressed, size_t compressed_len,
                                char *out_buf, size_t *out_len)
{
    if (!compressed || !out_buf || !out_len)
        return CWH_ERR_PARSE;

    z_stream stream = {0};
    stream.next_in = (Bytef *)compressed;
    stream.avail_in = (uInt)compressed_len;
    stream.next_out = (Bytef *)out_buf;
    stream.avail_out = (uInt)(*out_len);

    // Initialize for gzip decompression (window bits = 15 + 16)
    // 15 = max window size, +16 = gzip format
    int ret = inflateInit2(&stream, 15 + 16);
    if (ret != Z_OK)
        return CWH_ERR_PARSE;

    // Decompress
    ret = inflate(&stream, Z_FINISH);
    if (ret != Z_STREAM_END)
    {
        inflateEnd(&stream);
        return CWH_ERR_PARSE;
    }

    *out_len = stream.total_out;
    inflateEnd(&stream);
    return CWH_OK;
}

// Decompress deflate-compressed data
// deflate uses raw zlib (window bits = -15 for raw deflate)
cwh_error_t cwh_decompress_deflate(const char *compressed, size_t compressed_len,
                                   char *out_buf, size_t *out_len)
{
    if (!compressed || !out_buf || !out_len)
        return CWH_ERR_PARSE;

    z_stream stream = {0};
    stream.next_in = (Bytef *)compressed;
    stream.avail_in = (uInt)compressed_len;
    stream.next_out = (Bytef *)out_buf;
    stream.avail_out = (uInt)(*out_len);

    // Initialize for deflate decompression (window bits = -15 for raw deflate)
    // Negative value means raw deflate (no zlib header)
    int ret = inflateInit2(&stream, -15);
    if (ret != Z_OK)
        return CWH_ERR_PARSE;

    // Decompress
    ret = inflate(&stream, Z_FINISH);
    if (ret != Z_STREAM_END)
    {
        inflateEnd(&stream);
        return CWH_ERR_PARSE;
    }

    *out_len = stream.total_out;
    inflateEnd(&stream);
    return CWH_OK;
}

// ============================================================================
// HTTP/1.1 Server Implementation
// ============================================================================

// Create and bind server socket
cwh_server_t *cwh_listen(const char *addr_port, int backlog)
{
    if (!addr_port)
        return NULL;

#if defined(_WIN32) || defined(_WIN64)
    if (init_winsock() != 0)
        return NULL;
#endif

    // Parse addr:port (simple format: "8080" or "localhost:8080")
    char host[256] = "0.0.0.0"; // Default: bind to all interfaces
    int port = 8080;            // Default port

    // Try to parse port from addr_port
    const char *colon = strchr(addr_port, ':');
    if (colon)
    {
        // Has host:port format
        size_t host_len = colon - addr_port;
        if (host_len < sizeof(host))
        {
            memcpy(host, addr_port, host_len);
            host[host_len] = '\0';
        }
        port = atoi(colon + 1);
    }
    else
    {
        // Just port number
        port = atoi(addr_port);
    }

    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        return NULL;

    // Set SO_REUSEADDR to allow quick restart
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    // Bind to address and port
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    addr.sin_addr.s_addr = INADDR_ANY; // Bind to all interfaces

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        CLOSE_SOCKET(sock);
        return NULL;
    }

    // Listen for connections
    if (listen(sock, backlog) < 0)
    {
        CLOSE_SOCKET(sock);
        return NULL;
    }

    // Allocate server structure
    cwh_server_t *srv = malloc(sizeof(cwh_server_t));
    if (!srv)
    {
        CLOSE_SOCKET(sock);
        return NULL;
    }

    srv->sock = sock;
    srv->routes = NULL;

    return srv;
}

// Add route to server
cwh_error_t cwh_route(cwh_server_t *srv, const char *method, const char *pattern,
                      cwh_handler_t handler, void *user_data)
{
    if (!srv || !handler)
        return CWH_ERR_PARSE;

    // Allocate route
    cwh_route_t *route = malloc(sizeof(cwh_route_t));
    if (!route)
        return CWH_ERR_ALLOC;

    // Copy method (can be NULL for any method)
    route->method = method ? strdup(method) : NULL;
    // Copy pattern (can be NULL for wildcard matching)
    route->pattern = pattern ? strdup(pattern) : NULL;
    route->handler = handler;
    route->user_data = user_data;
    route->next = NULL;

    // Add to linked list (at end for predictable ordering)
    if (!srv->routes)
    {
        srv->routes = route;
    }
    else
    {
        cwh_route_t *r = srv->routes;
        while (r->next)
            r = r->next;
        r->next = route;
    }

    return CWH_OK;
}

// Find matching route for request
static cwh_route_t *find_route(cwh_server_t *srv, cwh_request_t *req)
{
    for (cwh_route_t *r = srv->routes; r; r = r->next)
    {
        // Check method match (NULL method = any method)
        if (r->method && strncasecmp(r->method, req->method_str, strlen(r->method)) != 0)
            continue;

        // Check pattern match (NULL pattern = match any path, otherwise exact match)
        if (r->pattern == NULL || strcmp(r->pattern, req->path) == 0)
            return r;
    }
    return NULL;
}

// Run server event loop (blocking)
cwh_error_t cwh_run(cwh_server_t *srv)
{
    if (!srv || srv->sock < 0)
        return CWH_ERR_NET;

    printf("Server listening on socket %d...\n", srv->sock);

    while (1)
    {
        // Accept connection
        struct sockaddr_in client_addr = {0};
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(srv->sock, (struct sockaddr *)&client_addr, &client_len);

        if (client_sock < 0)
        {
            // Accept failed (could be interrupt or shutdown)
            continue;
        }

        // Read request
        char req_buf[8192] = {0};
        int n = recv(client_sock, req_buf, sizeof(req_buf) - 1, 0);
        if (n <= 0)
        {
            CLOSE_SOCKET(client_sock);
            continue;
        }
        req_buf[n] = '\0';

        // Parse request
        cwh_request_t req = {0};
        if (cwh_parse_req(req_buf, n, &req) != CWH_OK || !req.is_valid)
        {
            // Send 400 Bad Request
            const char *bad_req = "HTTP/1.1 400 Bad Request\r\nContent-Length: 11\r\n\r\nBad Request";
            send(client_sock, bad_req, strlen(bad_req), 0);
            CLOSE_SOCKET(client_sock);
            continue;
        }

        // Log request
        printf("%s %s\n", req.method_str, req.path);
        fflush(stdout);

        // Create connection object for handler
        cwh_conn_t conn = {0};
        conn.fd = client_sock;
        conn.host = "client"; // Could parse from client_addr
        conn.port = 0;

        // Find and call handler
        cwh_route_t *route = find_route(srv, &req);
        if (route)
        {
            // Call handler
            route->handler(&req, &conn, route->user_data);
        }
        else
        {
            // 404 Not Found
            const char *not_found = "HTTP/1.1 404 Not Found\r\nContent-Length: 9\r\n\r\nNot Found";
            send(client_sock, not_found, strlen(not_found), 0);
        }

        // Close connection
        CLOSE_SOCKET(client_sock);
    }

    return CWH_OK;
}

// Free server and all routes
void cwh_free_server(cwh_server_t *srv)
{
    if (!srv)
        return;

    // Close socket
    if (srv->sock >= 0)
        CLOSE_SOCKET(srv->sock);

    // Free all routes
    cwh_route_t *r = srv->routes;
    while (r)
    {
        cwh_route_t *next = r->next;
        free(r->method);
        free(r->pattern);
        free(r);
        r = next;
    }

    free(srv);
}

// Server response helpers
cwh_error_t cwh_send_response(cwh_conn_t *conn, int status, const char *content_type,
                              const char *body, size_t body_len)
{
    if (!conn || conn->fd < 0)
        return CWH_ERR_NET;

    // Build response
    char resp_buf[16384];
    size_t offset = 0;

    // Status line
    offset += snprintf(resp_buf + offset, sizeof(resp_buf) - offset,
                       "HTTP/1.1 %d OK\r\n", status);

    // Headers
    if (content_type)
    {
        offset += snprintf(resp_buf + offset, sizeof(resp_buf) - offset,
                           "Content-Type: %s\r\n", content_type);
    }

    offset += snprintf(resp_buf + offset, sizeof(resp_buf) - offset,
                       "Content-Length: %lu\r\n", (unsigned long)body_len);

    offset += snprintf(resp_buf + offset, sizeof(resp_buf) - offset, "\r\n");

    // Body
    if (body && body_len > 0)
    {
        memcpy(resp_buf + offset, body, body_len);
        offset += body_len;
    }

    // Send
    send(conn->fd, resp_buf, offset, 0);

    return CWH_OK;
}

cwh_error_t cwh_send_status(cwh_conn_t *conn, int status, const char *message)
{
    if (!message)
        message = "OK";

    return cwh_send_response(conn, status, "text/plain", message, strlen(message));
}

// ============================================================================
// Static File Serving
// ============================================================================

// MIME type mapping (common types)
const char *cwh_get_mime_type(const char *path)
{
    if (!path)
        return "application/octet-stream";

    // Find last dot for extension
    const char *ext = strrchr(path, '.');
    if (!ext)
        return "application/octet-stream";

    ext++; // Skip the dot

    // Common text formats
    if (strcasecmp(ext, "html") == 0 || strcasecmp(ext, "htm") == 0)
        return "text/html";
    if (strcasecmp(ext, "css") == 0)
        return "text/css";
    if (strcasecmp(ext, "js") == 0)
        return "text/javascript";
    if (strcasecmp(ext, "json") == 0)
        return "application/json";
    if (strcasecmp(ext, "xml") == 0)
        return "application/xml";
    if (strcasecmp(ext, "txt") == 0)
        return "text/plain";

    // Images
    if (strcasecmp(ext, "png") == 0)
        return "image/png";
    if (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0)
        return "image/jpeg";
    if (strcasecmp(ext, "gif") == 0)
        return "image/gif";
    if (strcasecmp(ext, "svg") == 0)
        return "image/svg+xml";
    if (strcasecmp(ext, "ico") == 0)
        return "image/x-icon";
    if (strcasecmp(ext, "webp") == 0)
        return "image/webp";

    // Fonts
    if (strcasecmp(ext, "woff") == 0)
        return "font/woff";
    if (strcasecmp(ext, "woff2") == 0)
        return "font/woff2";
    if (strcasecmp(ext, "ttf") == 0)
        return "font/ttf";
    if (strcasecmp(ext, "otf") == 0)
        return "font/otf";

    // Archives
    if (strcasecmp(ext, "zip") == 0)
        return "application/zip";
    if (strcasecmp(ext, "gz") == 0)
        return "application/gzip";
    if (strcasecmp(ext, "tar") == 0)
        return "application/x-tar";

    // Documents
    if (strcasecmp(ext, "pdf") == 0)
        return "application/pdf";

    // Default
    return "application/octet-stream";
}

// Send a file to the client
cwh_error_t cwh_send_file(cwh_conn_t *conn, const char *file_path)
{
    if (!conn || !file_path)
        return CWH_ERR_PARSE;

    // Open file
    FILE *fp = fopen(file_path, "rb");
    if (!fp)
        return cwh_send_status(conn, 404, "File Not Found");

    // Get file size
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (file_size < 0 || file_size > 10 * 1024 * 1024) // Max 10MB
    {
        fclose(fp);
        return cwh_send_status(conn, 413, "File Too Large");
    }

    // Allocate buffer for file
    char *file_data = (char *)malloc((size_t)file_size);
    if (!file_data)
    {
        fclose(fp);
        return CWH_ERR_ALLOC;
    }

    // Read file
    size_t bytes_read = fread(file_data, 1, (size_t)file_size, fp);
    fclose(fp);

    if (bytes_read != (size_t)file_size)
    {
        free(file_data);
        return cwh_send_status(conn, 500, "File Read Error");
    }

    // Get MIME type
    const char *mime_type = cwh_get_mime_type(file_path);

    // Send file
    cwh_error_t err = cwh_send_response(conn, 200, mime_type, file_data, bytes_read);

    free(file_data);
    return err;
}

// Parse Range header (supports "bytes=start-end" format)
// Returns 1 if range parsed successfully, 0 if no range or invalid
static int parse_range_header(const char *range_header, size_t file_size,
                              size_t *out_start, size_t *out_end)
{
    if (!range_header || !out_start || !out_end)
        return 0;

    // Check for "bytes=" prefix
    if (strncmp(range_header, "bytes=", 6) != 0)
        return 0;

    const char *range_spec = range_header + 6;

    // Parse start-end format
    long long start = -1, end = -1;

    // Handle different range formats:
    // "bytes=100-200" - from byte 100 to 200
    // "bytes=100-" - from byte 100 to end
    // "bytes=-200" - last 200 bytes

    if (range_spec[0] == '-')
    {
        // Suffix range: last N bytes
        end = file_size - 1;
        start = file_size - atoll(range_spec + 1);
        if (start < 0)
            start = 0;
    }
    else
    {
        // Parse start
        char *dash = strchr(range_spec, '-');
        if (!dash)
            return 0;

        start = atoll(range_spec);

        // Parse end (if present)
        if (*(dash + 1) == '\0' || *(dash + 1) == ' ' || *(dash + 1) == '\r' || *(dash + 1) == '\n')
        {
            // Open-ended range (e.g., "50-")
            end = file_size - 1;
        }
        else
        {
            end = atoll(dash + 1);
        }
    }

    // Validate range
    if (start < 0 || start >= (long long)file_size || end < start)
        return 0;

    if (end >= (long long)file_size)
        end = file_size - 1;

    *out_start = (size_t)start;
    *out_end = (size_t)end;

    return 1;
}

// Send file with Range request support (HTTP 206 Partial Content)
cwh_error_t cwh_send_file_range(cwh_conn_t *conn, const char *file_path,
                                const char *range_header)
{
    if (!conn || !file_path)
        return CWH_ERR_PARSE;

    FILE *fp = fopen(file_path, "rb");
    if (!fp)
        return cwh_send_status(conn, 404, "File Not Found");

    // Get file size
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (file_size < 0 || file_size > 10 * 1024 * 1024) // Max 10MB
    {
        fclose(fp);
        return cwh_send_status(conn, 413, "File Too Large");
    }

    // Parse range if present
    size_t range_start = 0, range_end = (size_t)file_size - 1;
    int is_range_request = 0;

    if (range_header)
    {
        is_range_request = parse_range_header(range_header, (size_t)file_size,
                                              &range_start, &range_end);
    }

    // Calculate content length
    size_t content_length = is_range_request ? (range_end - range_start + 1) : (size_t)file_size;

    // Allocate buffer for content
    char *file_data = (char *)malloc(content_length);
    if (!file_data)
    {
        fclose(fp);
        return CWH_ERR_ALLOC;
    }

    // Seek to start position and read
    if (is_range_request)
    {
        fseek(fp, (long)range_start, SEEK_SET);
    }

    size_t bytes_read = fread(file_data, 1, content_length, fp);
    fclose(fp);

    if (bytes_read != content_length)
    {
        free(file_data);
        return cwh_send_status(conn, 500, "File Read Error");
    }

    // Build response
    char resp_buf[16384];
    size_t offset = 0;

    // Status line
    if (is_range_request)
    {
        offset += snprintf(resp_buf + offset, sizeof(resp_buf) - offset,
                           "HTTP/1.1 206 Partial Content\r\n");
    }
    else
    {
        offset += snprintf(resp_buf + offset, sizeof(resp_buf) - offset,
                           "HTTP/1.1 200 OK\r\n");
    }

    // Headers
    const char *mime_type = cwh_get_mime_type(file_path);
    offset += snprintf(resp_buf + offset, sizeof(resp_buf) - offset,
                       "Content-Type: %s\r\n", mime_type);

    offset += snprintf(resp_buf + offset, sizeof(resp_buf) - offset,
                       "Content-Length: %lu\r\n", (unsigned long)content_length);

    offset += snprintf(resp_buf + offset, sizeof(resp_buf) - offset,
                       "Accept-Ranges: bytes\r\n");

    if (is_range_request)
    {
        offset += snprintf(resp_buf + offset, sizeof(resp_buf) - offset,
                           "Content-Range: bytes %lu-%lu/%ld\r\n",
                           (unsigned long)range_start, (unsigned long)range_end, file_size);
    }

    offset += snprintf(resp_buf + offset, sizeof(resp_buf) - offset, "\r\n");

    // Send headers
    send(conn->fd, resp_buf, offset, 0);

    // Send body
    send(conn->fd, file_data, content_length, 0);

    free(file_data);
    return CWH_OK;
}

// Handler for serving static files from a directory
// Usage: cwh_route(srv, "GET", "/*", cwh_serve_static, "/path/to/www");
cwh_error_t cwh_serve_static(cwh_request_t *req, cwh_conn_t *conn, void *root_dir)
{
    if (!req || !conn || !root_dir)
        return cwh_send_status(conn, 500, "Internal Server Error");

    const char *root = (const char *)root_dir;
    const char *path = req->path;

    // Security: prevent directory traversal
    if (strstr(path, "..") != NULL)
        return cwh_send_status(conn, 403, "Forbidden");

    // Build full file path
    char file_path[1024];
    snprintf(file_path, sizeof(file_path), "%s%s", root, path);

    // If path ends with /, serve index.html
    size_t path_len = strlen(file_path);
    if (path_len > 0 && file_path[path_len - 1] == '/')
    {
        snprintf(file_path + path_len, sizeof(file_path) - path_len, "index.html");
    }

    printf("Serving file: %s\n", file_path);
    fflush(stdout);

    // Check for Range header
    const char *range_header = cwh_get_header(req, "Range");

    // Use range-aware file sending
    return cwh_send_file_range(conn, file_path, range_header);
}

// ============================================================================
// High-level convenience API
// ============================================================================

// Internal helper for high-level requests
// Maximum number of redirects to follow (prevent infinite loops)
#define CWH_MAX_REDIRECTS 10

// Helper function to check if a status code is a redirect
static int is_redirect_status(int status)
{
    return (status == 301 || status == 302 || status == 303 ||
            status == 307 || status == 308);
}

// Helper function to determine if the request method should change on redirect
// 302/303 change POST to GET, others preserve the method
static cwh_method_t get_redirect_method(int status, cwh_method_t original_method)
{
    if ((status == 302 || status == 303) && original_method == CWH_METHOD_POST)
        return CWH_METHOD_GET;

    return original_method;
}

static cwh_error_t cwh_request_simple(const char *url, cwh_method_t method,
                                      const char *body, size_t body_len,
                                      cwh_response_t *res)
{
    if (!url || !res)
        return CWH_ERR_PARSE;

    // Redirect tracking
    int redirect_count = 0;
    char visited_urls[CWH_MAX_REDIRECTS][2048] = {0};

    // Current request parameters
    const char *current_url = url;
    cwh_method_t current_method = method;
    const char *current_body = body;
    size_t current_body_len = body_len;

    // Response buffer for redirects
    static char redirect_url_buf[2048];

    while (redirect_count < CWH_MAX_REDIRECTS)
    {
        // Parse URL to extract path
        cwh_url_t parsed = {0};
        if (cwh_parse_url(current_url, strlen(current_url), &parsed) != CWH_OK)
            return CWH_ERR_PARSE;

        // Connect
        cwh_conn_t *conn = cwh_connect(current_url, 5000);
        if (!conn)
            return CWH_ERR_NET;

        // Extract path (or use "/" as default)
        const char *path = "/";
        static char path_buf[2048];
        if (parsed.path)
        {
            // Find path length
            const char *path_end = parsed.path;
            while (*path_end && *path_end != '?' && *path_end != '#')
                path_end++;

            // Create null-terminated path string
            size_t path_len = path_end - parsed.path;
            if (path_len >= sizeof(path_buf))
                path_len = sizeof(path_buf) - 1;

            memcpy(path_buf, parsed.path, path_len);
            path_buf[path_len] = '\0';
            path = path_buf;
        }

        // Send request
        cwh_error_t err = cwh_send_req(conn, current_method, path, NULL,
                                       current_body, current_body_len);
        if (err != CWH_OK)
        {
            cwh_close(conn);
            return err;
        }

        // Read response
        err = cwh_read_res(conn, res);
        cwh_close(conn);

        if (err != CWH_OK)
            return err;

        // Check if this is a redirect
        if (!is_redirect_status(res->status))
        {
            // Not a redirect, return the response
            return CWH_OK;
        }

        // Get Location header for redirect
        const char *location = cwh_get_res_header(res, "Location");
        if (!location)
        {
            // Redirect without Location header - return error
            return CWH_ERR_PARSE;
        }

        // Check for circular redirects - store visited URL
        size_t url_len = strlen(current_url);
        if (url_len >= sizeof(visited_urls[redirect_count]))
            url_len = sizeof(visited_urls[redirect_count]) - 1;

        memcpy(visited_urls[redirect_count], current_url, url_len);
        visited_urls[redirect_count][url_len] = '\0';

        // Handle relative vs absolute Location URLs
        // If Location starts with '/', it's relative - build absolute URL
        if (location[0] == '/')
        {
            // Relative URL - combine with current host
            // Format: scheme://host:port/path
            int len = snprintf(redirect_url_buf, sizeof(redirect_url_buf),
                              "%s://%s", parsed.scheme, parsed.host);

            if (parsed.port_str)
            {
                len += snprintf(redirect_url_buf + len, sizeof(redirect_url_buf) - len,
                               ":%s", parsed.port_str);
            }

            // Add the relative path
            size_t loc_len = strlen(location);
            if ((size_t)len + loc_len >= sizeof(redirect_url_buf))
                loc_len = sizeof(redirect_url_buf) - len - 1;

            memcpy(redirect_url_buf + len, location, loc_len);
            redirect_url_buf[len + loc_len] = '\0';
        }
        else
        {
            // Absolute URL - use as-is
            size_t loc_len = strlen(location);
            if (loc_len >= sizeof(redirect_url_buf))
                loc_len = sizeof(redirect_url_buf) - 1;

            memcpy(redirect_url_buf, location, loc_len);
            redirect_url_buf[loc_len] = '\0';
        }

        // Check if we've already visited this URL (circular redirect)
        for (int i = 0; i <= redirect_count; i++)
        {
            if (strcmp(visited_urls[i], redirect_url_buf) == 0)
            {
                // Circular redirect detected
                return CWH_ERR_PARSE;
            }
        }

        // Update method based on redirect type
        current_method = get_redirect_method(res->status, current_method);

        // For 303 and method changes, drop the body
        if (current_method != method)
        {
            current_body = NULL;
            current_body_len = 0;
        }

        // Follow the redirect
        current_url = redirect_url_buf;
        redirect_count++;
    }

    // Too many redirects
    return CWH_ERR_PARSE;
}

// One-liner GET request
cwh_error_t cwh_get(const char *url, cwh_response_t *res)
{
    return cwh_request_simple(url, CWH_METHOD_GET, NULL, 0, res);
}

// One-liner POST request
cwh_error_t cwh_post(const char *url, const char *body, size_t body_len, cwh_response_t *res)
{
    return cwh_request_simple(url, CWH_METHOD_POST, body, body_len, res);
}

// One-liner PUT request
cwh_error_t cwh_put(const char *url, const char *body, size_t body_len, cwh_response_t *res)
{
    return cwh_request_simple(url, CWH_METHOD_PUT, body, body_len, res);
}

// One-liner DELETE request
cwh_error_t cwh_delete(const char *url, cwh_response_t *res)
{
    return cwh_request_simple(url, CWH_METHOD_DELETE, NULL, 0, res);
}