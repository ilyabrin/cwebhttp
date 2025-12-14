// loop.c - Cross-platform event loop abstraction
// Unified API for epoll/kqueue/IOCP/select backends

#include "../../include/cwebhttp_async.h"
#include <stdlib.h>
#include <string.h>

// Platform detection
#if defined(__linux__)
#define USE_EPOLL
#include <sys/epoll.h>
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#define USE_KQUEUE
#include <sys/event.h>
#elif defined(_WIN32) || defined(_WIN64)
#define USE_IOCP
#include <windows.h>
#else
#define USE_SELECT
#include <sys/select.h>
#endif

// Forward declarations for backend functions
#ifdef USE_EPOLL
typedef struct cwh_epoll cwh_epoll_t;
cwh_epoll_t *cwh_epoll_create(int max_events);
int cwh_epoll_add(cwh_epoll_t *ep, int fd, int events, cwh_event_cb cb, void *data);
int cwh_epoll_mod(cwh_epoll_t *ep, int fd, int events);
int cwh_epoll_del(cwh_epoll_t *ep, int fd);
int cwh_epoll_wait(cwh_epoll_t *ep, int timeout_ms);
int cwh_epoll_run(cwh_epoll_t *ep);
void cwh_epoll_stop(cwh_epoll_t *ep);
void cwh_epoll_free(cwh_epoll_t *ep);
const char *cwh_epoll_backend(void);
#endif

// Event loop structure (opaque to users)
struct cwh_loop
{
    void *backend;    // Platform-specific backend
    int backend_type; // Backend type identifier
    int running;      // Loop running flag
};

// Backend type constants
#define BACKEND_EPOLL 1
#define BACKEND_KQUEUE 2
#define BACKEND_IOCP 3
#define BACKEND_SELECT 4

// Create new event loop
cwh_loop_t *cwh_loop_new(void)
{
    cwh_loop_t *loop = (cwh_loop_t *)calloc(1, sizeof(cwh_loop_t));
    if (!loop)
        return NULL;

#ifdef USE_EPOLL
    loop->backend = cwh_epoll_create(1024); // Default: 1024 max events
    loop->backend_type = BACKEND_EPOLL;
#elif defined(USE_KQUEUE)
    // TODO: Implement kqueue backend
    loop->backend = NULL;
    loop->backend_type = BACKEND_KQUEUE;
#elif defined(USE_IOCP)
    // TODO: Implement IOCP backend
    loop->backend = NULL;
    loop->backend_type = BACKEND_IOCP;
#else
    // TODO: Implement select backend
    loop->backend = NULL;
    loop->backend_type = BACKEND_SELECT;
#endif

    if (!loop->backend)
    {
        free(loop);
        return NULL;
    }

    loop->running = 0;
    return loop;
}

// Run event loop (blocking until stopped)
int cwh_loop_run(cwh_loop_t *loop)
{
    if (!loop || !loop->backend)
        return -1;

    loop->running = 1;

#ifdef USE_EPOLL
    if (loop->backend_type == BACKEND_EPOLL)
    {
        return cwh_epoll_run((cwh_epoll_t *)loop->backend);
    }
#endif

    return -1;
}

// Run one iteration of event loop (non-blocking)
int cwh_loop_run_once(cwh_loop_t *loop, int timeout_ms)
{
    if (!loop || !loop->backend)
        return -1;

#ifdef USE_EPOLL
    if (loop->backend_type == BACKEND_EPOLL)
    {
        return cwh_epoll_wait((cwh_epoll_t *)loop->backend, timeout_ms);
    }
#endif

    return -1;
}

// Stop event loop
void cwh_loop_stop(cwh_loop_t *loop)
{
    if (!loop || !loop->backend)
        return;

    loop->running = 0;

#ifdef USE_EPOLL
    if (loop->backend_type == BACKEND_EPOLL)
    {
        cwh_epoll_stop((cwh_epoll_t *)loop->backend);
    }
#endif
}

// Cleanup event loop
void cwh_loop_free(cwh_loop_t *loop)
{
    if (!loop)
        return;

#ifdef USE_EPOLL
    if (loop->backend_type == BACKEND_EPOLL && loop->backend)
    {
        cwh_epoll_free((cwh_epoll_t *)loop->backend);
    }
#endif

    free(loop);
}

// Register file descriptor for events
int cwh_loop_add(cwh_loop_t *loop, int fd, int events, cwh_event_cb cb, void *data)
{
    if (!loop || !loop->backend || fd < 0 || !cb)
        return -1;

#ifdef USE_EPOLL
    if (loop->backend_type == BACKEND_EPOLL)
    {
        return cwh_epoll_add((cwh_epoll_t *)loop->backend, fd, events, cb, data);
    }
#endif

    return -1;
}

// Modify events for file descriptor
int cwh_loop_mod(cwh_loop_t *loop, int fd, int events)
{
    if (!loop || !loop->backend || fd < 0)
        return -1;

#ifdef USE_EPOLL
    if (loop->backend_type == BACKEND_EPOLL)
    {
        return cwh_epoll_mod((cwh_epoll_t *)loop->backend, fd, events);
    }
#endif

    return -1;
}

// Remove file descriptor from loop
int cwh_loop_del(cwh_loop_t *loop, int fd)
{
    if (!loop || !loop->backend || fd < 0)
        return -1;

#ifdef USE_EPOLL
    if (loop->backend_type == BACKEND_EPOLL)
    {
        return cwh_epoll_del((cwh_epoll_t *)loop->backend, fd);
    }
#endif

    return -1;
}

// Get backend name (for debugging)
const char *cwh_loop_backend(cwh_loop_t *loop)
{
    if (!loop)
        return "unknown";

#ifdef USE_EPOLL
    if (loop->backend_type == BACKEND_EPOLL)
    {
        return cwh_epoll_backend();
    }
#elif defined(USE_KQUEUE)
    if (loop->backend_type == BACKEND_KQUEUE)
    {
        return "kqueue (macOS/BSD)";
    }
#elif defined(USE_IOCP)
    if (loop->backend_type == BACKEND_IOCP)
    {
        return "IOCP (Windows)";
    }
#else
    if (loop->backend_type == BACKEND_SELECT)
    {
        return "select (fallback)";
    }
#endif

    return "unknown";
}
