#include "cwebhttp.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#define strncasecmp _strnicmp
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h> // для Linux/macOS
#include <unistd.h>    // close
#include <strings.h>   // strncasecmp on Unix
#endif

// Definition of cwh_method_strs
const char *cwh_method_strs[CWH_METHOD_NUM + 1] = {
    "GET", "POST", "PUT", "DELETE", NULL};

// Dummy connect (реальный потом с getaddrinfo)
cwh_conn_t *cwh_connect(const char *url, int timeout_ms)
{
    (void)timeout_ms; // ignore for now
    cwh_conn_t *conn = malloc(sizeof(cwh_conn_t));
    if (!conn)
        return NULL;
    conn->fd = -1; // dummy
    conn->host = strdup(url ? url : "localhost");
    conn->port = 80;
    return conn;
}

cwh_error_t cwh_send_req(cwh_conn_t *conn, cwh_method_t method, const char *path, const char **headers, const char *body, size_t body_len)
{
    (void)conn;
    (void)method;
    (void)path;
    (void)headers;
    (void)body;
    (void)body_len;
    if (conn->fd < 0)
        return CWH_ERR_NET;
    return CWH_OK; // dummy
}

cwh_error_t cwh_read_res(cwh_conn_t *conn, cwh_response_t *res)
{
    (void)conn;
    (void)res;
    res->status = 200;
    res->body = "Dummy response";
    res->body_len = strlen(res->body);
    return CWH_OK;
}

void cwh_close(cwh_conn_t *conn)
{
    if (conn)
    {
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

// Сервер dummy
cwh_server_t *cwh_listen(const char *addr_port, int backlog)
{
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