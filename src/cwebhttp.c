#if !defined(_WIN32) && !defined(_WIN64)
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif

#include "cwebhttp.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <ws2tcpip.h>
#define strncasecmp _strnicmp
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
    cwh_conn_t *conn = malloc(sizeof(cwh_conn_t));
    if (!conn)
    {
        CLOSE_SOCKET(sock);
        return NULL;
    }

    conn->fd = sock;
    conn->host = strdup(host);
    conn->port = parsed.port;

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
    return cwh_parse_res(recv_buf, n, res);
}

void cwh_close(cwh_conn_t *conn)
{
    if (conn)
    {
        if (conn->fd >= 0)
        {
            CLOSE_SOCKET(conn->fd);
        }
        free(conn->host);
        free(conn);
    }
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

// Сервер dummy
cwh_server_t *cwh_listen(const char *addr_port, int backlog)
{
    (void)addr_port;
    (void)backlog;
    cwh_server_t *srv = malloc(sizeof(cwh_server_t));
    if (!srv)
        return NULL;
    srv->sock = -1; // dummy bind
    return srv;
}

cwh_error_t cwh_route(cwh_server_t *srv, const char *method, const char *pattern, cwh_handler_t handler, void *user_data)
{
    (void)srv;
    (void)method;
    (void)pattern;
    (void)handler;
    (void)user_data;
    return CWH_OK;
}

cwh_error_t cwh_run(cwh_server_t *srv)
{
    (void)srv;
    printf("Dummy server running...\n");
#if defined(_WIN32) || defined(_WIN64)
    Sleep(5000); // Windows Sleep takes milliseconds
#else
    sleep(5); // Unix sleep takes seconds
#endif
    return CWH_OK;
}

void cwh_free_server(cwh_server_t *srv)
{
    free(srv);
}

// ============================================================================
// High-level convenience API
// ============================================================================

// Internal helper for high-level requests
static cwh_error_t cwh_request_simple(const char *url, cwh_method_t method,
                                      const char *body, size_t body_len,
                                      cwh_response_t *res)
{
    if (!url || !res)
        return CWH_ERR_PARSE;

    // Parse URL to extract path
    cwh_url_t parsed = {0};
    if (cwh_parse_url(url, strlen(url), &parsed) != CWH_OK)
        return CWH_ERR_PARSE;

    // Connect
    cwh_conn_t *conn = cwh_connect(url, 5000);
    if (!conn)
        return CWH_ERR_NET;

    // Extract path (or use "/" as default)
    const char *path = "/";
    if (parsed.path)
    {
        // Find path length
        const char *path_end = parsed.path;
        while (*path_end && *path_end != '?' && *path_end != '#')
            path_end++;

        // Create null-terminated path string
        static char path_buf[2048];
        size_t path_len = path_end - parsed.path;
        if (path_len >= sizeof(path_buf))
            path_len = sizeof(path_buf) - 1;

        memcpy(path_buf, parsed.path, path_len);
        path_buf[path_len] = '\0';
        path = path_buf;
    }

    // Send request
    cwh_error_t err = cwh_send_req(conn, method, path, NULL, body, body_len);
    if (err != CWH_OK)
    {
        cwh_close(conn);
        return err;
    }

    // Read response
    err = cwh_read_res(conn, res);

    // Close connection
    cwh_close(conn);

    return err;
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