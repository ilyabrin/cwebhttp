// client.c - Async HTTP client implementation
// Non-blocking HTTP requests using event loop

#ifndef _WIN32
#define _POSIX_C_SOURCE 200112L
#define _GNU_SOURCE
#endif

#include "../../include/cwebhttp_async.h"
#include "../../include/cwebhttp.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#ifndef _WIN32
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <strings.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

// ============================================================================
// Async Request State Machine
// ============================================================================

typedef enum
{
    ASYNC_STATE_IDLE,
    ASYNC_STATE_DNS,
    ASYNC_STATE_CONNECTING,
    ASYNC_STATE_SENDING,
    ASYNC_STATE_RECEIVING,
    ASYNC_STATE_COMPLETE,
    ASYNC_STATE_ERROR
} cwh_async_state_t;

// Async request context
typedef struct cwh_async_request
{
    // State
    cwh_async_state_t state;
    cwh_loop_t *loop;

    // Request details
    cwh_method_t method;
    char url[2048];
    cwh_url_t parsed_url;
    const char **headers;
    char *body;
    size_t body_len;

    // Network
    int fd;
    struct sockaddr_in addr;

    // Buffers
    char send_buf[16384];
    size_t send_len;
    size_t send_offset;

    char recv_buf[65536];
    size_t recv_len;

    // Response
    cwh_response_t response;

    // Callback
    cwh_async_cb callback;
    void *user_data;

    // Linked list for tracking
    struct cwh_async_request *next;
} cwh_async_request_t;

// Global list of active requests
static cwh_async_request_t *active_requests = NULL;

// ============================================================================
// Request Lifecycle Management
// ============================================================================

// Allocate new async request
static cwh_async_request_t *alloc_request(cwh_loop_t *loop)
{
    cwh_async_request_t *req = (cwh_async_request_t *)calloc(1, sizeof(cwh_async_request_t));
    if (!req)
        return NULL;

    req->loop = loop;
    req->fd = -1;
    req->state = ASYNC_STATE_IDLE;

    // Add to active requests list
    req->next = active_requests;
    active_requests = req;

    return req;
}

// Remove request from active list
static void remove_from_active_list(cwh_async_request_t *req)
{
    cwh_async_request_t **curr = &active_requests;
    while (*curr)
    {
        if (*curr == req)
        {
            *curr = req->next;
            return;
        }
        curr = &(*curr)->next;
    }
}

// Cleanup and free request
static void cleanup_request(cwh_async_request_t *req)
{
    if (!req)
        return;

    // Remove from event loop
    if (req->fd >= 0)
    {
        cwh_loop_del(req->loop, req->fd);
#ifdef _WIN32
        closesocket(req->fd);
#else
        close(req->fd);
#endif
    }

    // Remove from active list
    remove_from_active_list(req);

    // Free allocated memory
    free(req->body);
    free(req);
}

// ============================================================================
// DNS Resolution (Blocking for now, async in future)
// ============================================================================

static int resolve_host(const char *host, struct sockaddr_in *addr)
{
    struct addrinfo hints = {0};
    struct addrinfo *result = NULL;

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int ret = getaddrinfo(host, NULL, &hints, &result);
    if (ret != 0)
        return -1;

    if (!result || result->ai_family != AF_INET)
    {
        freeaddrinfo(result);
        return -1;
    }

    *addr = *(struct sockaddr_in *)result->ai_addr;
    freeaddrinfo(result);

    return 0;
}

// ============================================================================
// Connection Management
// ============================================================================

// Check if non-blocking connect completed
static int check_connect_complete(int fd)
{
    int error = 0;
    socklen_t len = sizeof(error);

#ifdef _WIN32
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (char *)&error, &len) < 0)
        return -1;
#else
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0)
        return -1;
#endif

    if (error != 0)
        return -1;

    return 0;
}

// Start async connection
static int start_connect(cwh_async_request_t *req)
{
    // Extract null-terminated hostname
    // The URL parser uses zero-alloc pointers, so we need to copy the host
    char hostname[256] = {0};
    const char *host_start = req->parsed_url.host;
    const char *host_end = host_start;

    // Find end of hostname (before '/', '?', '#', ':', or null)
    while (*host_end && *host_end != '/' && *host_end != '?' && *host_end != '#' && *host_end != ':')
    {
        host_end++;
    }

    size_t host_len = host_end - host_start;
    if (host_len >= sizeof(hostname))
        return -1;

    memcpy(hostname, host_start, host_len);
    hostname[host_len] = '\0';

    // DNS resolution (blocking for now, async DNS planned for future)
    if (resolve_host(hostname, &req->addr) < 0)
        return -1;

    // Set port AFTER resolve_host (which overwrites the whole struct)
    req->addr.sin_family = AF_INET;
    req->addr.sin_port = htons(req->parsed_url.port);

    // Create socket
    req->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (req->fd < 0)
        return -1;

    // Set non-blocking
    if (cwh_set_nonblocking(req->fd) < 0)
    {
#ifdef _WIN32
        closesocket(req->fd);
#else
        close(req->fd);
#endif
        req->fd = -1;
        return -1;
    }

    // Initiate connect
    int ret = connect(req->fd, (struct sockaddr *)&req->addr, sizeof(req->addr));

    if (ret < 0)
    {
#ifdef _WIN32
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK || err == WSAEINPROGRESS)
        {
#else
        if (errno == EINPROGRESS)
        {
#endif
            // Connect in progress - register for WRITE event
            req->state = ASYNC_STATE_CONNECTING;
            return 0;
        }

        // Connect failed
#ifdef _WIN32
        closesocket(req->fd);
#else
        close(req->fd);
#endif
        req->fd = -1;
        return -1;
    }

    // Connected immediately (rare)
    req->state = ASYNC_STATE_SENDING;
    return 0;
}

// ============================================================================
// Request Sending
// ============================================================================

// Format HTTP request
static int format_request(cwh_async_request_t *req)
{
    cwh_request_t http_req = {0};

    // Set method string based on method enum
    http_req.method_str = (char *)cwh_method_strs[req->method];
    http_req.path = req->parsed_url.path;
    http_req.query = req->parsed_url.query;

    // Build headers array (key-value pairs: headers[i*2] = key, headers[i*2+1] = value)
    char host_header_key[] = "Host";
    static char host_header_val[512];
    if (req->parsed_url.port == 80)
    {
        snprintf(host_header_val, sizeof(host_header_val), "%s", req->parsed_url.host);
    }
    else
    {
        snprintf(host_header_val, sizeof(host_header_val), "%s:%d",
                 req->parsed_url.host, req->parsed_url.port);
    }

    int header_idx = 0;
    http_req.headers[header_idx++] = host_header_key;
    http_req.headers[header_idx++] = host_header_val;
    http_req.headers[header_idx++] = "Connection";
    http_req.headers[header_idx++] = "close"; // For simplicity, close connection after request

    // Add user headers (expecting key-value pairs)
    if (req->headers)
    {
        for (int i = 0; req->headers[i] && header_idx < 30; i += 2)
        {
            http_req.headers[header_idx++] = (char *)req->headers[i];     // key
            http_req.headers[header_idx++] = (char *)req->headers[i + 1]; // value
        }
    }

    http_req.num_headers = header_idx / 2;
    http_req.body = req->body;
    http_req.body_len = req->body_len;

    // Format into send buffer
    cwh_error_t err = cwh_format_req(req->send_buf, &req->send_len, &http_req);
    if (err != CWH_OK)
        return -1;

    req->send_offset = 0;
    return 0;
}

// Send request data (non-blocking)
static int send_request_data(cwh_async_request_t *req)
{
    while (req->send_offset < req->send_len)
    {
        int n = send(req->fd,
                     req->send_buf + req->send_offset,
                     (int)(req->send_len - req->send_offset),
                     0);

        if (n < 0)
        {
#ifdef _WIN32
            if (WSAGetLastError() == WSAEWOULDBLOCK)
            {
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
#endif
                // Would block - wait for WRITE event
                return 0;
            }
            // Send error
            return -1;
        }

        req->send_offset += n;
    }

    // All sent - switch to receiving
    req->state = ASYNC_STATE_RECEIVING;
    cwh_loop_mod(req->loop, req->fd, CWH_EVENT_READ);

    return 0;
}

// ============================================================================
// Response Receiving
// ============================================================================

// Check if response is complete
static int response_complete(const char *buf, size_t len)
{
    // Find end of headers
    const char *headers_end = strstr(buf, "\r\n\r\n");
    if (!headers_end)
        return 0; // Headers not complete

    // Check Content-Length
    const char *cl = strstr(buf, "Content-Length:");
    if (cl && cl < headers_end)
    {
        int content_length = atoi(cl + 15);
        size_t headers_len = (headers_end + 4) - buf;
        size_t total_len = headers_len + content_length;

        return (int)(len >= total_len);
    }

    // Check for chunked encoding
    if (strstr(buf, "Transfer-Encoding: chunked") || strstr(buf, "Transfer-Encoding:chunked"))
    {
        // Look for final chunk: "0\r\n\r\n"
        return strstr(headers_end + 4, "0\r\n\r\n") != NULL;
    }

    // No Content-Length and not chunked - assume complete after headers
    return 1;
}

// Receive response data (non-blocking)
static int recv_response_data(cwh_async_request_t *req)
{
    while (req->recv_len < sizeof(req->recv_buf) - 1)
    {
        int n = recv(req->fd,
                     req->recv_buf + req->recv_len,
                     (int)(sizeof(req->recv_buf) - req->recv_len - 1),
                     0);

        if (n < 0)
        {
#ifdef _WIN32
            if (WSAGetLastError() == WSAEWOULDBLOCK)
            {
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
#endif
                // Would block - wait for more data
                return 0;
            }
            // Receive error
            return -1;
        }

        if (n == 0)
        {
            // Connection closed
            break;
        }

        req->recv_len += n;
        req->recv_buf[req->recv_len] = '\0';

        // Check if response is complete
        if (response_complete(req->recv_buf, req->recv_len))
        {
            break;
        }
    }

    // Parse response
    cwh_error_t err = cwh_parse_res(req->recv_buf, req->recv_len, &req->response);

    // Invoke callback
    if (req->callback)
    {
        req->callback(&req->response, err, req->user_data);
    }

    // Cleanup
    cleanup_request(req);

    return 0;
}

// ============================================================================
// Event Handler
// ============================================================================

static void async_request_event_handler(cwh_loop_t *loop, int fd, int events, void *data)
{
    cwh_async_request_t *req = (cwh_async_request_t *)data;

    // Handle errors
    if (events & CWH_EVENT_ERROR)
    {
        if (req->callback)
        {
            req->callback(NULL, CWH_ERR_NET, req->user_data);
        }
        cleanup_request(req);
        return;
    }

    // State machine
    switch (req->state)
    {
    case ASYNC_STATE_CONNECTING:
        if (events & CWH_EVENT_WRITE)
        {
            // Check if connect completed
            if (check_connect_complete(req->fd) < 0)
            {
                // Connect failed
                if (req->callback)
                {
                    req->callback(NULL, CWH_ERR_NET, req->user_data);
                }
                cleanup_request(req);
                return;
            }

            // Connected! Format and send request
            if (format_request(req) < 0)
            {
                if (req->callback)
                {
                    req->callback(NULL, CWH_ERR_NET, req->user_data);
                }
                cleanup_request(req);
                return;
            }

            req->state = ASYNC_STATE_SENDING;
            // Fall through to send
        }
        else
        {
            break;
        }
        // Fall through intentionally

    case ASYNC_STATE_SENDING:
        if (events & CWH_EVENT_WRITE)
        {
            if (send_request_data(req) < 0)
            {
                if (req->callback)
                {
                    req->callback(NULL, CWH_ERR_NET, req->user_data);
                }
                cleanup_request(req);
                return;
            }
        }
        break;

    case ASYNC_STATE_RECEIVING:
        if (events & CWH_EVENT_READ)
        {
            if (recv_response_data(req) < 0)
            {
                if (req->callback)
                {
                    req->callback(NULL, CWH_ERR_NET, req->user_data);
                }
                cleanup_request(req);
                return;
            }
        }
        break;

    default:
        break;
    }
}

// ============================================================================
// Public API Implementation
// ============================================================================

// Generic async request
void cwh_async_request(cwh_loop_t *loop,
                       cwh_method_t method,
                       const char *url,
                       const char **headers,
                       const char *body,
                       size_t body_len,
                       cwh_async_cb cb,
                       void *data)
{
    if (!loop || !url || !cb)
        return;

    // Allocate request context
    cwh_async_request_t *req = alloc_request(loop);
    if (!req)
    {
        cb(NULL, CWH_ERR_NET, data);
        return;
    }

    // Initialize request
    req->method = method;
    req->callback = cb;
    req->user_data = data;
    req->headers = headers;

    // Copy URL
    strncpy(req->url, url, sizeof(req->url) - 1);
    req->url[sizeof(req->url) - 1] = '\0';

    // Parse URL
    if (cwh_parse_url(req->url, strlen(req->url), &req->parsed_url) != CWH_OK)
    {
        cb(NULL, CWH_ERR_PARSE, data);
        cleanup_request(req);
        return;
    }

    // Copy body if present
    if (body && body_len > 0)
    {
        req->body = (char *)malloc(body_len);
        if (!req->body)
        {
            cb(NULL, CWH_ERR_NET, data);
            cleanup_request(req);
            return;
        }
        memcpy(req->body, body, body_len);
        req->body_len = body_len;
    }

    // Start connection
    if (start_connect(req) < 0)
    {
        cb(NULL, CWH_ERR_NET, data);
        cleanup_request(req);
        return;
    }

    // Register with event loop
    int event_mask = (req->state == ASYNC_STATE_CONNECTING) ? CWH_EVENT_WRITE : CWH_EVENT_READ | CWH_EVENT_WRITE;
    if (cwh_loop_add(loop, req->fd, event_mask, async_request_event_handler, req) < 0)
    {
        cb(NULL, CWH_ERR_NET, data);
        cleanup_request(req);
        return;
    }
}

// Async GET request
void cwh_async_get(cwh_loop_t *loop, const char *url, cwh_async_cb cb, void *data)
{
    cwh_async_request(loop, CWH_METHOD_GET, url, NULL, NULL, 0, cb, data);
}

// Async POST request
void cwh_async_post(cwh_loop_t *loop, const char *url,
                    const char *body, size_t body_len,
                    cwh_async_cb cb, void *data)
{
    cwh_async_request(loop, CWH_METHOD_POST, url, NULL, body, body_len, cb, data);
}

// Async PUT request
void cwh_async_put(cwh_loop_t *loop, const char *url,
                   const char *body, size_t body_len,
                   cwh_async_cb cb, void *data)
{
    cwh_async_request(loop, CWH_METHOD_PUT, url, NULL, body, body_len, cb, data);
}

// Async DELETE request
void cwh_async_delete(cwh_loop_t *loop, const char *url, cwh_async_cb cb, void *data)
{
    cwh_async_request(loop, CWH_METHOD_DELETE, url, NULL, NULL, 0, cb, data);
}
