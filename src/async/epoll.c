// epoll.c - Linux epoll backend for async event loop
// High-performance I/O multiplexing for 10K+ concurrent connections

#include "../../include/cwebhttp_async.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef __linux__
#include <unistd.h>
#include <sys/epoll.h>

// Event handler entry
typedef struct cwh_event_entry
{
    int fd;
    int events;
    cwh_event_cb callback;
    void *data;
    struct cwh_event_entry *next;
} cwh_event_entry_t;

// epoll-based event loop
typedef struct cwh_epoll
{
    int epoll_fd;
    struct epoll_event *events;
    int max_events;
    cwh_event_entry_t *handlers; // Linked list of event handlers
    int running;
    void *loop_ptr; // Pointer back to cwh_loop_t for callbacks
} cwh_epoll_t;

// Create epoll instance
cwh_epoll_t *cwh_epoll_create(int max_events)
{
    cwh_epoll_t *ep = (cwh_epoll_t *)calloc(1, sizeof(cwh_epoll_t));
    if (!ep)
        return NULL;

    ep->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (ep->epoll_fd < 0)
    {
        free(ep);
        return NULL;
    }

    ep->events = (struct epoll_event *)calloc(max_events, sizeof(struct epoll_event));
    if (!ep->events)
    {
        close(ep->epoll_fd);
        free(ep);
        return NULL;
    }

    ep->max_events = max_events;
    ep->handlers = NULL;
    ep->running = 0;
    ep->loop_ptr = NULL; // Will be set by loop.c

    return ep;
}

// Find event handler by fd
static cwh_event_entry_t *find_handler(cwh_epoll_t *ep, int fd)
{
    cwh_event_entry_t *curr = ep->handlers;
    while (curr)
    {
        if (curr->fd == fd)
            return curr;
        curr = curr->next;
    }
    return NULL;
}

// Convert cwebhttp events to epoll events
static uint32_t cwh_to_epoll_events(int events)
{
    uint32_t ep_events = 0;
    if (events & CWH_EVENT_READ)
        ep_events |= EPOLLIN;
    if (events & CWH_EVENT_WRITE)
        ep_events |= EPOLLOUT;
    if (events & CWH_EVENT_ERROR)
        ep_events |= EPOLLERR;
    return ep_events;
}

// Convert epoll events to cwebhttp events
static int epoll_to_cwh_events(uint32_t ep_events)
{
    int events = 0;
    if (ep_events & EPOLLIN)
        events |= CWH_EVENT_READ;
    if (ep_events & EPOLLOUT)
        events |= CWH_EVENT_WRITE;
    if (ep_events & (EPOLLERR | EPOLLHUP))
        events |= CWH_EVENT_ERROR;
    return events;
}

// Add file descriptor to epoll
int cwh_epoll_add(cwh_epoll_t *ep, int fd, int events, cwh_event_cb cb, void *data)
{
    if (!ep || fd < 0 || !cb)
        return -1;

    // Check if already registered
    if (find_handler(ep, fd))
        return -1;

    // Create handler entry
    cwh_event_entry_t *entry = (cwh_event_entry_t *)calloc(1, sizeof(cwh_event_entry_t));
    if (!entry)
        return -1;

    entry->fd = fd;
    entry->events = events;
    entry->callback = cb;
    entry->data = data;

    // Add to epoll
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = cwh_to_epoll_events(events);
    ev.data.fd = fd;

    if (epoll_ctl(ep->epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0)
    {
        free(entry);
        return -1;
    }

    // Add to handler list
    entry->next = ep->handlers;
    ep->handlers = entry;

    return 0;
}

// Modify file descriptor events
int cwh_epoll_mod(cwh_epoll_t *ep, int fd, int events)
{
    if (!ep || fd < 0)
        return -1;

    // Find handler
    cwh_event_entry_t *entry = find_handler(ep, fd);
    if (!entry)
        return -1;

    // Update events
    entry->events = events;

    // Modify in epoll
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = cwh_to_epoll_events(events);
    ev.data.fd = fd;

    if (epoll_ctl(ep->epoll_fd, EPOLL_CTL_MOD, fd, &ev) < 0)
    {
        return -1;
    }

    return 0;
}

// Remove file descriptor from epoll
int cwh_epoll_del(cwh_epoll_t *ep, int fd)
{
    if (!ep || fd < 0)
        return -1;

    // Remove from epoll
    if (epoll_ctl(ep->epoll_fd, EPOLL_CTL_DEL, fd, NULL) < 0)
    {
        return -1;
    }

    // Remove from handler list
    cwh_event_entry_t *prev = NULL;
    cwh_event_entry_t *curr = ep->handlers;
    while (curr)
    {
        if (curr->fd == fd)
        {
            if (prev)
            {
                prev->next = curr->next;
            }
            else
            {
                ep->handlers = curr->next;
            }
            free(curr);
            return 0;
        }
        prev = curr;
        curr = curr->next;
    }

    return -1;
}

// Wait for events and dispatch callbacks
int cwh_epoll_wait(cwh_epoll_t *ep, int timeout_ms)
{
    if (!ep)
        return -1;

    int nfds = epoll_wait(ep->epoll_fd, ep->events, ep->max_events, timeout_ms);
    if (nfds < 0)
    {
        if (errno == EINTR)
            return 0; // Interrupted, not an error
        return -1;
    }

    // Dispatch callbacks
    for (int i = 0; i < nfds; i++)
    {
        int fd = ep->events[i].data.fd;
        cwh_event_entry_t *entry = find_handler(ep, fd);
        if (entry && entry->callback)
        {
            int events = epoll_to_cwh_events(ep->events[i].events);
            entry->callback((cwh_loop_t *)ep->loop_ptr, fd, events, entry->data);
        }
    }

    return nfds;
}

// Stop epoll loop
void cwh_epoll_stop(cwh_epoll_t *ep)
{
    if (ep)
    {
        ep->running = 0;
    }
}

// Run epoll loop
int cwh_epoll_run(cwh_epoll_t *ep)
{
    if (!ep)
        return -1;

    ep->running = 1;
    while (ep->running)
    {
        int ret = cwh_epoll_wait(ep, -1); // Block indefinitely
        if (ret < 0)
            return -1;
    }

    return 0;
}

// Cleanup epoll
void cwh_epoll_free(cwh_epoll_t *ep)
{
    if (!ep)
        return;

    // Free handler list
    cwh_event_entry_t *curr = ep->handlers;
    while (curr)
    {
        cwh_event_entry_t *next = curr->next;
        free(curr);
        curr = next;
    }

    // Close epoll fd
    if (ep->epoll_fd >= 0)
    {
        close(ep->epoll_fd);
    }

    // Free events array
    free(ep->events);
    free(ep);
}

// Set loop pointer (called by loop.c after creation)
void cwh_epoll_set_loop(cwh_epoll_t *ep, void *loop)
{
    if (ep)
    {
        ep->loop_ptr = loop;
    }
}

// Get backend name
const char *cwh_epoll_backend(void)
{
    return "epoll (Linux)";
}

#endif // __linux__
