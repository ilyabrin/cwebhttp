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
// TODO: Implement IOCP, using select as fallback for now
#define USE_SELECT
#include <winsock2.h>
#else
#define USE_SELECT
#include <sys/select.h>
#endif

// Forward declarations for backend functions
#ifdef USE_EPOLL
typedef struct cwh_epoll cwh_epoll_t;
cwh_epoll_t *cwh_epoll_create(int max_events);
void cwh_epoll_set_loop(cwh_epoll_t *ep, void *loop);
int cwh_epoll_add(cwh_epoll_t *ep, int fd, int events, cwh_event_cb cb, void *data);
int cwh_epoll_mod(cwh_epoll_t *ep, int fd, int events);
int cwh_epoll_del(cwh_epoll_t *ep, int fd);
int cwh_epoll_wait(cwh_epoll_t *ep, int timeout_ms);
int cwh_epoll_run(cwh_epoll_t *ep);
void cwh_epoll_stop(cwh_epoll_t *ep);
void cwh_epoll_free(cwh_epoll_t *ep);
const char *cwh_epoll_backend(void);
#elif defined(USE_KQUEUE)
typedef struct cwh_kqueue cwh_kqueue_t;
cwh_kqueue_t *cwh_kqueue_create(int max_events);
void cwh_kqueue_set_loop(cwh_kqueue_t *kq, void *loop);
int cwh_kqueue_add(cwh_kqueue_t *kq, int fd, int events, cwh_event_cb cb, void *data);
int cwh_kqueue_mod(cwh_kqueue_t *kq, int fd, int events);
int cwh_kqueue_del(cwh_kqueue_t *kq, int fd);
int cwh_kqueue_wait(cwh_kqueue_t *kq, int timeout_ms);
int cwh_kqueue_run(cwh_kqueue_t *kq);
void cwh_kqueue_stop(cwh_kqueue_t *kq);
void cwh_kqueue_free(cwh_kqueue_t *kq);
const char *cwh_kqueue_backend(void);
#elif defined(USE_SELECT)
typedef struct cwh_select cwh_select_t;
cwh_select_t *cwh_select_create(void);
void cwh_select_set_loop(cwh_select_t *sel, void *loop);
int cwh_select_add(cwh_select_t *sel, int fd, int events, cwh_event_cb cb, void *data);
int cwh_select_mod(cwh_select_t *sel, int fd, int events);
int cwh_select_del(cwh_select_t *sel, int fd);
int cwh_select_wait(cwh_select_t *sel, int timeout_ms);
int cwh_select_run(cwh_select_t *sel);
void cwh_select_stop(cwh_select_t *sel);
void cwh_select_free(cwh_select_t *sel);
const char *cwh_select_backend(void);
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
    if (loop->backend)
    {
        cwh_epoll_set_loop((cwh_epoll_t *)loop->backend, loop);
    }
#elif defined(USE_KQUEUE)
    loop->backend = cwh_kqueue_create(1024); // Default: 1024 max events
    loop->backend_type = BACKEND_KQUEUE;
    if (loop->backend)
    {
        cwh_kqueue_set_loop((cwh_kqueue_t *)loop->backend, loop);
    }
#elif defined(USE_IOCP)
    // TODO: Implement IOCP backend
    loop->backend = NULL;
    loop->backend_type = BACKEND_IOCP;
#else
    // Use select as fallback
    loop->backend = cwh_select_create();
    loop->backend_type = BACKEND_SELECT;
    if (loop->backend)
    {
        cwh_select_set_loop((cwh_select_t *)loop->backend, loop);
    }
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
#elif defined(USE_KQUEUE)
    if (loop->backend_type == BACKEND_KQUEUE)
    {
        return cwh_kqueue_run((cwh_kqueue_t *)loop->backend);
    }
#elif defined(USE_SELECT)
    if (loop->backend_type == BACKEND_SELECT)
    {
        return cwh_select_run((cwh_select_t *)loop->backend);
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
#elif defined(USE_KQUEUE)
    if (loop->backend_type == BACKEND_KQUEUE)
    {
        return cwh_kqueue_wait((cwh_kqueue_t *)loop->backend, timeout_ms);
    }
#elif defined(USE_SELECT)
    if (loop->backend_type == BACKEND_SELECT)
    {
        return cwh_select_wait((cwh_select_t *)loop->backend, timeout_ms);
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
#elif defined(USE_KQUEUE)
    if (loop->backend_type == BACKEND_KQUEUE)
    {
        cwh_kqueue_stop((cwh_kqueue_t *)loop->backend);
    }
#elif defined(USE_SELECT)
    if (loop->backend_type == BACKEND_SELECT)
    {
        cwh_select_stop((cwh_select_t *)loop->backend);
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
#elif defined(USE_KQUEUE)
    if (loop->backend_type == BACKEND_KQUEUE && loop->backend)
    {
        cwh_kqueue_free((cwh_kqueue_t *)loop->backend);
    }
#elif defined(USE_SELECT)
    if (loop->backend_type == BACKEND_SELECT && loop->backend)
    {
        cwh_select_free((cwh_select_t *)loop->backend);
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
#elif defined(USE_KQUEUE)
    if (loop->backend_type == BACKEND_KQUEUE)
    {
        return cwh_kqueue_add((cwh_kqueue_t *)loop->backend, fd, events, cb, data);
    }
#elif defined(USE_SELECT)
    if (loop->backend_type == BACKEND_SELECT)
    {
        return cwh_select_add((cwh_select_t *)loop->backend, fd, events, cb, data);
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
#elif defined(USE_KQUEUE)
    if (loop->backend_type == BACKEND_KQUEUE)
    {
        return cwh_kqueue_mod((cwh_kqueue_t *)loop->backend, fd, events);
    }
#elif defined(USE_SELECT)
    if (loop->backend_type == BACKEND_SELECT)
    {
        return cwh_select_mod((cwh_select_t *)loop->backend, fd, events);
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
#elif defined(USE_KQUEUE)
    if (loop->backend_type == BACKEND_KQUEUE)
    {
        return cwh_kqueue_del((cwh_kqueue_t *)loop->backend, fd);
    }
#elif defined(USE_SELECT)
    if (loop->backend_type == BACKEND_SELECT)
    {
        return cwh_select_del((cwh_select_t *)loop->backend, fd);
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
        return cwh_kqueue_backend();
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
