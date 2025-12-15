// server.c - Async HTTP Server Implementation
// High-performance event-driven HTTP/1.1 server with C10K capability

#include "../../include/cwebhttp_async.h"
#include "../../include/cwebhttp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#endif

// ============================================================================
// Async Route Structure
// ============================================================================

typedef struct cwh_async_route
{
    cwh_method_t method;          // HTTP method (GET, POST, etc.)
    const char *path;             // Route path pattern
    cwh_async_handler_t handler;  // Route handler function
    void *user_data;              // User data for handler
    struct cwh_async_route *next; // Linked list
} cwh_async_route_t;

// ============================================================================
// Connection State Machine
// ============================================================================

typedef enum
{
    CONN_STATE_NEW,
    CONN_STATE_READING_REQUEST,
    CONN_STATE_PROCESSING,
    CONN_STATE_WRITING_RESPONSE,
    CONN_STATE_KEEPALIVE,
    CONN_STATE_CLOSED
} cwh_conn_state_t;

// ============================================================================
// Connection Structure
// ============================================================================

typedef struct cwh_async_conn
{
    int fd;                          // Client socket
    cwh_conn_state_t state;          // Connection state
    struct cwh_async_server *server; // Back pointer to server

    // Request data
    char recv_buf[16384];  // Request buffer (16KB)
    size_t recv_len;       // Bytes received
    cwh_request_t request; // Parsed request
    bool request_complete; // Request fully received

    // Response data
    char send_buf[65536]; // Response buffer (64KB)
    size_t send_len;      // Response size
    size_t send_offset;   // Bytes already sent

    // Timing
    time_t last_activity; // Last I/O timestamp
    int timeout_ms;       // Connection timeout (default: 30000)

    // Keep-alive
    bool keep_alive;     // Connection: keep-alive
    int requests_served; // Requests on this connection

    struct cwh_async_conn *next; // Linked list
} cwh_async_conn_t;

// ============================================================================
// Server Structure
// ============================================================================

struct cwh_async_server
{
    cwh_loop_t *loop;          // Event loop
    int listen_fd;             // Listening socket
    int port;                  // Server port
    bool running;              // Server running flag
    cwh_async_route_t *routes; // Route handlers (linked list)

    // Connection management
    cwh_async_conn_t *connections; // Active connections (linked list)
    int conn_count;                // Current connection count
    int max_connections;           // Max concurrent connections (default: 10000)

    // Statistics
    uint64_t total_requests;    // Total requests handled
    uint64_t total_connections; // Total connections accepted
};

// ============================================================================
// Forward Declarations
// ============================================================================

static void connection_event_handler(cwh_loop_t *loop, int fd, int events, void *data);
static void listen_event_handler(cwh_loop_t *loop, int fd, int events, void *data);
static cwh_async_conn_t *create_connection(cwh_async_server_t *server, int client_fd);
static void close_connection(cwh_async_conn_t *conn);
static int read_request(cwh_async_conn_t *conn);
static int write_response(cwh_async_conn_t *conn);
static void process_request(cwh_async_conn_t *conn);
static cwh_async_route_t *find_route(cwh_async_server_t *server, cwh_method_t method, const char *path);

// ============================================================================
// Server Lifecycle
// ============================================================================

// Create async server
cwh_async_server_t *cwh_async_server_new(cwh_loop_t *loop)
{
    if (!loop)
        return NULL;

    cwh_async_server_t *server = (cwh_async_server_t *)calloc(1, sizeof(cwh_async_server_t));
    if (!server)
        return NULL;

    server->loop = loop;
    server->listen_fd = -1;
    server->port = 0;
    server->running = false;
    server->routes = NULL;
    server->connections = NULL;
    server->conn_count = 0;
    server->max_connections = 10000; // C10K capable
    server->total_requests = 0;
    server->total_connections = 0;

    return server;
}

// Start listening on port (non-blocking)
int cwh_async_listen(cwh_async_server_t *server, int port)
{
    if (!server || port <= 0 || port > 65535)
        return -1;

    // Create listening socket
    server->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->listen_fd < 0)
        return -1;

    // Set SO_REUSEADDR to avoid "Address already in use" errors
    int reuse = 1;
#ifdef _WIN32
    if (setsockopt(server->listen_fd, SOL_SOCKET, SO_REUSEADDR,
                   (char *)&reuse, sizeof(reuse)) < 0)
#else
    if (setsockopt(server->listen_fd, SOL_SOCKET, SO_REUSEADDR,
                   &reuse, sizeof(reuse)) < 0)
#endif
    {
#ifdef _WIN32
        closesocket(server->listen_fd);
#else
        close(server->listen_fd);
#endif
        server->listen_fd = -1;
        return -1;
    }

    // Set non-blocking
    if (cwh_set_nonblocking(server->listen_fd) < 0)
    {
#ifdef _WIN32
        closesocket(server->listen_fd);
#else
        close(server->listen_fd);
#endif
        server->listen_fd = -1;
        return -1;
    }

    // Bind to port
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server->listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
#ifdef _WIN32
        closesocket(server->listen_fd);
#else
        close(server->listen_fd);
#endif
        server->listen_fd = -1;
        return -1;
    }

    // Listen with backlog
    if (listen(server->listen_fd, 128) < 0)
    {
#ifdef _WIN32
        closesocket(server->listen_fd);
#else
        close(server->listen_fd);
#endif
        server->listen_fd = -1;
        return -1;
    }

    server->port = port;
    server->running = true;

    // Register listen socket for READ events (accept)
    if (cwh_loop_add(server->loop, server->listen_fd, CWH_EVENT_READ,
                     listen_event_handler, server) < 0)
    {
#ifdef _WIN32
        closesocket(server->listen_fd);
#else
        close(server->listen_fd);
#endif
        server->listen_fd = -1;
        server->running = false;
        return -1;
    }

    return 0;
}

// Stop server gracefully
void cwh_async_server_stop(cwh_async_server_t *server)
{
    if (!server || !server->running)
        return;

    server->running = false;

    // Close listening socket
    if (server->listen_fd >= 0)
    {
        cwh_loop_del(server->loop, server->listen_fd);
#ifdef _WIN32
        closesocket(server->listen_fd);
#else
        close(server->listen_fd);
#endif
        server->listen_fd = -1;
    }

    // Close all connections
    cwh_async_conn_t *conn = server->connections;
    while (conn)
    {
        cwh_async_conn_t *next = conn->next;
        close_connection(conn);
        conn = next;
    }
}

// Free server resources
void cwh_async_server_free(cwh_async_server_t *server)
{
    if (!server)
        return;

    // Stop server first
    cwh_async_server_stop(server);

    // Free routes
    cwh_async_route_t *route = server->routes;
    while (route)
    {
        cwh_async_route_t *next = route->next;
        free((void *)route->path);
        free(route);
        route = next;
    }

    free(server);
}

// ============================================================================
// Routing
// ============================================================================

// Convert method string to enum
static cwh_method_t parse_method(const char *method_str)
{
    if (!method_str)
        return CWH_METHOD_GET;
    if (strcmp(method_str, "GET") == 0)
        return CWH_METHOD_GET;
    if (strcmp(method_str, "POST") == 0)
        return CWH_METHOD_POST;
    if (strcmp(method_str, "PUT") == 0)
        return CWH_METHOD_PUT;
    if (strcmp(method_str, "DELETE") == 0)
        return CWH_METHOD_DELETE;
    return CWH_METHOD_GET; // Default
}

// Register route handler
void cwh_async_route(cwh_async_server_t *server,
                     const char *method,
                     const char *path,
                     cwh_async_handler_t handler,
                     void *user_data)
{
    if (!server || !path || !handler)
        return;

    cwh_async_route_t *route = (cwh_async_route_t *)calloc(1, sizeof(cwh_async_route_t));
    if (!route)
        return;

    // Parse method string to enum
    route->method = CWH_METHOD_GET; // Default
    if (method)
    {
        if (strcmp(method, "GET") == 0)
            route->method = CWH_METHOD_GET;
        else if (strcmp(method, "POST") == 0)
            route->method = CWH_METHOD_POST;
        else if (strcmp(method, "PUT") == 0)
            route->method = CWH_METHOD_PUT;
        else if (strcmp(method, "DELETE") == 0)
            route->method = CWH_METHOD_DELETE;
    }

    route->path = strdup(path);
    route->handler = handler;
    route->user_data = user_data;
    route->next = server->routes;

    server->routes = route;
}

// Find matching route
static cwh_async_route_t *find_route(cwh_async_server_t *server, cwh_method_t method, const char *path)
{
    cwh_async_route_t *route = server->routes;

    while (route)
    {
        // Check method match
        if (route->method != method)
        {
            route = route->next;
            continue;
        }

        // Check path (exact match for now)
        if (strcmp(route->path, path) == 0)
        {
            return route;
        }

        route = route->next;
    }

    return NULL;
}

// ============================================================================
// Connection Management
// ============================================================================

// Create new connection
static cwh_async_conn_t *create_connection(cwh_async_server_t *server, int client_fd)
{
    cwh_async_conn_t *conn = (cwh_async_conn_t *)calloc(1, sizeof(cwh_async_conn_t));
    if (!conn)
        return NULL;

    conn->fd = client_fd;
    conn->state = CONN_STATE_READING_REQUEST;
    conn->server = server;
    conn->recv_len = 0;
    conn->request_complete = false;
    conn->send_len = 0;
    conn->send_offset = 0;
    conn->last_activity = time(NULL);
    conn->timeout_ms = 30000; // 30 seconds
    conn->keep_alive = false;
    conn->requests_served = 0;

    // Add to server's connection list
    conn->next = server->connections;
    server->connections = conn;
    server->conn_count++;
    server->total_connections++;

    return conn;
}

// Close and free connection
static void close_connection(cwh_async_conn_t *conn)
{
    if (!conn)
        return;

    cwh_async_server_t *server = conn->server;

    // Remove from event loop
    cwh_loop_del(server->loop, conn->fd);

    // Close socket
#ifdef _WIN32
    closesocket(conn->fd);
#else
    close(conn->fd);
#endif

    // Remove from server's connection list
    cwh_async_conn_t **p = &server->connections;
    while (*p)
    {
        if (*p == conn)
        {
            *p = conn->next;
            break;
        }
        p = &(*p)->next;
    }

    server->conn_count--;
    free(conn);
}

// ============================================================================
// Event Handlers
// ============================================================================

// Listen socket event handler (accept new connections)
static void listen_event_handler(cwh_loop_t *loop, int fd, int events, void *data)
{
    (void)loop;
    (void)fd;
    (void)events;

    cwh_async_server_t *server = (cwh_async_server_t *)data;
    printf("[DEBUG] listen_event_handler called\n");

    // Accept multiple connections in a loop (batch accept)
    while (server->running && server->conn_count < server->max_connections)
    {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        int client_fd = accept(server->listen_fd,
                               (struct sockaddr *)&client_addr,
                               &addr_len);

        printf("[DEBUG] accept() returned: %d\n", client_fd);

        if (client_fd < 0)
        {
#ifdef _WIN32
            if (WSAGetLastError() == WSAEWOULDBLOCK)
                break; // No more connections to accept
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break; // No more connections to accept
#endif
            continue; // Error, try next
        }

        // Set non-blocking
        if (cwh_set_nonblocking(client_fd) < 0)
        {
#ifdef _WIN32
            closesocket(client_fd);
#else
            close(client_fd);
#endif
            continue;
        }

        // Create connection
        cwh_async_conn_t *conn = create_connection(server, client_fd);
        if (!conn)
        {
#ifdef _WIN32
            closesocket(client_fd);
#else
            close(client_fd);
#endif
            continue;
        }

        // Register for READ events
        if (cwh_loop_add(server->loop, client_fd, CWH_EVENT_READ,
                         connection_event_handler, conn) < 0)
        {
            close_connection(conn);
            continue;
        }
    }
}

// Connection event handler
static void connection_event_handler(cwh_loop_t *loop, int fd, int events, void *data)
{
    (void)loop;
    (void)fd;

    cwh_async_conn_t *conn = (cwh_async_conn_t *)data;

    // Update last activity
    conn->last_activity = time(NULL);

    // Handle errors
    if (events & CWH_EVENT_ERROR)
    {
        close_connection(conn);
        return;
    }

    // State machine
    switch (conn->state)
    {
    case CONN_STATE_READING_REQUEST:
        if (events & CWH_EVENT_READ)
        {
            int result = read_request(conn);
            if (result < 0)
            {
                close_connection(conn);
                return;
            }

            if (conn->request_complete)
            {
                conn->state = CONN_STATE_PROCESSING;
                process_request(conn);
            }
        }
        break;

    case CONN_STATE_WRITING_RESPONSE:
        if (events & CWH_EVENT_WRITE)
        {
            int result = write_response(conn);
            if (result < 0)
            {
                close_connection(conn);
                return;
            }

            if (result == 0)
            {
                // Response fully sent
                if (conn->keep_alive)
                {
                    // Reset for next request
                    conn->state = CONN_STATE_READING_REQUEST;
                    conn->recv_len = 0;
                    conn->send_len = 0;
                    conn->send_offset = 0;
                    conn->request_complete = false;
                    memset(&conn->request, 0, sizeof(conn->request));

                    // Switch to READ events
                    cwh_loop_mod(conn->server->loop, conn->fd, CWH_EVENT_READ);
                }
                else
                {
                    close_connection(conn);
                }
            }
        }
        break;

    default:
        break;
    }
}

// ============================================================================
// Request/Response I/O
// ============================================================================

// Read request data (non-blocking)
static int read_request(cwh_async_conn_t *conn)
{
    ssize_t n = recv(conn->fd,
                     conn->recv_buf + conn->recv_len,
                     sizeof(conn->recv_buf) - conn->recv_len - 1,
                     0);

    if (n > 0)
    {
        conn->recv_len += n;
        conn->recv_buf[conn->recv_len] = '\0';

        // Try to parse request
        if (cwh_parse_req(conn->recv_buf, conn->recv_len, &conn->request) == CWH_OK)
        {
            conn->request_complete = true;

            // Check for keep-alive
            const char *connection_header = cwh_get_header(&conn->request, "connection");
            if (connection_header && strcasecmp(connection_header, "keep-alive") == 0)
            {
                conn->keep_alive = true;
            }

            return 0;
        }

        // Need more data (request incomplete)
        return 1;
    }

    if (n == 0)
    {
        return -1; // Client closed connection
    }

#ifdef _WIN32
    if (WSAGetLastError() == WSAEWOULDBLOCK)
        return 1; // Would block, wait for more data
#else
    if (errno == EAGAIN || errno == EWOULDBLOCK)
        return 1; // Would block, wait for more data
#endif

    return -1; // Error
}

// Write response data (non-blocking)
static int write_response(cwh_async_conn_t *conn)
{
    ssize_t n = send(conn->fd,
                     conn->send_buf + conn->send_offset,
                     conn->send_len - conn->send_offset,
                     0);

    if (n > 0)
    {
        conn->send_offset += n;

        if (conn->send_offset >= conn->send_len)
        {
            return 0; // All sent
        }

        return 1; // More to send
    }

#ifdef _WIN32
    if (n < 0 && WSAGetLastError() == WSAEWOULDBLOCK)
        return 1; // Would block, wait for writable
#else
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        return 1; // Would block, wait for writable
#endif

    return -1; // Error
}

// Process request and generate response
static void process_request(cwh_async_conn_t *conn)
{
    cwh_async_server_t *server = conn->server;
    server->total_requests++;
    conn->requests_served++;

    // Convert method string to enum
    cwh_method_t method = parse_method(conn->request.method_str);

    // Find matching route
    cwh_async_route_t *route = find_route(server, method, conn->request.path);

    if (route)
    {
        // Call handler
        route->handler(conn, &conn->request, route->user_data);
    }
    else
    {
        // 404 Not Found
        cwh_async_send_status(conn, 404, "Not Found");
    }
}

// ============================================================================
// Response Helpers
// ============================================================================

// Send HTTP response
void cwh_async_send_response(cwh_async_conn_t *conn,
                             int status,
                             const char *content_type,
                             const char *body,
                             size_t body_len)
{
    if (!conn)
        return;

    // Build response
    int written = snprintf(conn->send_buf, sizeof(conn->send_buf),
                           "HTTP/1.1 %d OK\r\n"
                           "Content-Type: %s\r\n"
                           "Content-Length: %lu\r\n"
                           "%s"
                           "\r\n",
                           status,
                           content_type,
                           (unsigned long)body_len,
                           conn->keep_alive ? "Connection: keep-alive\r\n" : "Connection: close\r\n");

    if (written < 0 || (size_t)written >= sizeof(conn->send_buf))
        return;

    // Add body
    if (body && body_len > 0)
    {
        size_t available = sizeof(conn->send_buf) - written;
        if (body_len > available)
            body_len = available;

        memcpy(conn->send_buf + written, body, body_len);
        written += body_len;
    }

    conn->send_len = written;
    conn->send_offset = 0;

    // Switch to WRITING_RESPONSE state and register for WRITE events
    conn->state = CONN_STATE_WRITING_RESPONSE;
    cwh_loop_mod(conn->server->loop, conn->fd, CWH_EVENT_WRITE);
}

// Send status response
void cwh_async_send_status(cwh_async_conn_t *conn, int status, const char *message)
{
    if (!conn || !message)
        return;

    char body[256];
    int body_len = snprintf(body, sizeof(body),
                            "<html><body><h1>%d %s</h1></body></html>",
                            status, message);

    if (body_len > 0)
    {
        cwh_async_send_response(conn, status, "text/html", body, body_len);
    }
}

// Send JSON response
void cwh_async_send_json(cwh_async_conn_t *conn, int status, const char *json)
{
    if (!conn || !json)
        return;

    cwh_async_send_response(conn, status, "application/json", json, strlen(json));
}
