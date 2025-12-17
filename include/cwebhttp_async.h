// cwebhttp_async.h - Async I/O API for cwebhttp
// Event-driven HTTP client and server with 10K+ concurrent connections

#ifndef CWEBHTTP_ASYNC_H
#define CWEBHTTP_ASYNC_H

#include "cwebhttp.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // ============================================================================
    // Event Loop
    // ============================================================================

    // Opaque event loop handle
    typedef struct cwh_loop cwh_loop_t;

    // Event types for I/O operations
    typedef enum
    {
        CWH_EVENT_READ = 0x01,  // Socket ready for reading
        CWH_EVENT_WRITE = 0x02, // Socket ready for writing
        CWH_EVENT_ERROR = 0x04  // Socket error occurred
    } cwh_event_type_t;

    // Event callback - called when fd is ready
    typedef void (*cwh_event_cb)(cwh_loop_t *loop, int fd, int events, void *data);

    // Create new event loop
    cwh_loop_t *cwh_loop_new(void);

    // Run event loop (blocking until stopped)
    // Returns 0 on success, -1 on error
    int cwh_loop_run(cwh_loop_t *loop);

    // Run one iteration of event loop (non-blocking)
    // Returns number of events processed, -1 on error
    int cwh_loop_run_once(cwh_loop_t *loop, int timeout_ms);

    // Stop event loop
    void cwh_loop_stop(cwh_loop_t *loop);

    // Cleanup event loop
    void cwh_loop_free(cwh_loop_t *loop);

    // Register file descriptor for events
    int cwh_loop_add(cwh_loop_t *loop, int fd, int events, cwh_event_cb cb, void *data);

    // Modify events for file descriptor
    int cwh_loop_mod(cwh_loop_t *loop, int fd, int events);

    // Remove file descriptor from loop
    int cwh_loop_del(cwh_loop_t *loop, int fd);

    // Get backend name (for debugging)
    const char *cwh_loop_backend(cwh_loop_t *loop);

    // Get accepted socket from listen socket (IOCP AcceptEx integration)
    // Returns accepted socket fd, or -1 if none available or not using IOCP
    // Internal function used by async server
    int cwh_loop_get_accepted_socket(cwh_loop_t *loop, int listen_fd);

    // ============================================================================
    // Async Client API
    // ============================================================================

    // Async response callback
    typedef void (*cwh_async_cb)(cwh_response_t *res, cwh_error_t err, void *data);

    // Async GET request
    void cwh_async_get(cwh_loop_t *loop, const char *url, cwh_async_cb cb, void *data);

    // Async POST request
    void cwh_async_post(cwh_loop_t *loop, const char *url,
                        const char *body, size_t body_len,
                        cwh_async_cb cb, void *data);

    // Async PUT request
    void cwh_async_put(cwh_loop_t *loop, const char *url,
                       const char *body, size_t body_len,
                       cwh_async_cb cb, void *data);

    // Async DELETE request
    void cwh_async_delete(cwh_loop_t *loop, const char *url, cwh_async_cb cb, void *data);

    // Async request with custom method and headers
    void cwh_async_request(cwh_loop_t *loop,
                           cwh_method_t method,
                           const char *url,
                           const char **headers,
                           const char *body,
                           size_t body_len,
                           cwh_async_cb cb,
                           void *data);

    // ============================================================================
    // Connection Pool Management
    // ============================================================================

    // Initialize connection pool with custom settings
    // max_connections: Maximum number of pooled connections (default: 50)
    // idle_timeout_sec: Seconds before idle connections are closed (default: 300)
    void cwh_async_pool_init(int max_connections, int idle_timeout_sec);

    // Get connection pool statistics
    void cwh_async_pool_stats(int *active_count, int *total_count);

    // Cleanup expired connections from pool
    void cwh_async_pool_cleanup(void);

    // Shutdown and free connection pool
    void cwh_async_pool_shutdown(void);

    // ============================================================================
    // Async Server API
    // ============================================================================

    // Opaque async server handle
    typedef struct cwh_async_server cwh_async_server_t;

    // Opaque async connection handle
    typedef struct cwh_async_conn cwh_async_conn_t;

    // Async request handler
    typedef void (*cwh_async_handler_t)(cwh_async_conn_t *conn, cwh_request_t *req, void *data);

    // Create async server
    cwh_async_server_t *cwh_async_server_new(cwh_loop_t *loop);

    // Register route handler
    void cwh_async_route(cwh_async_server_t *server,
                         const char *method,
                         const char *path,
                         cwh_async_handler_t handler,
                         void *data);

    // Start listening (non-blocking)
    int cwh_async_listen(cwh_async_server_t *server, int port);

    // Stop server
    void cwh_async_server_stop(cwh_async_server_t *server);

    // Free server
    void cwh_async_server_free(cwh_async_server_t *server);

    // Response helpers
    void cwh_async_send_response(cwh_async_conn_t *conn,
                                 int status,
                                 const char *content_type,
                                 const char *body,
                                 size_t body_len);

    void cwh_async_send_status(cwh_async_conn_t *conn,
                               int status,
                               const char *message);

    void cwh_async_send_json(cwh_async_conn_t *conn,
                             int status,
                             const char *json);

    // ============================================================================
    // Utilities
    // ============================================================================

    // Set socket to non-blocking mode
    int cwh_set_nonblocking(int fd);

    // Set socket to blocking mode
    int cwh_set_blocking(int fd);

#ifdef __cplusplus
}
#endif

#endif // CWEBHTTP_ASYNC_H
