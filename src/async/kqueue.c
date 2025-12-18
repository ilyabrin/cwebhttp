// kqueue.c - BSD/macOS kqueue backend for async event loop
// High-performance I/O multiplexing for macOS, FreeBSD, OpenBSD, NetBSD

#include "../../include/cwebhttp_async.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#include <unistd.h>
#include <sys/event.h>
#include <sys/time.h>

// Event handler entry
typedef struct cwh_event_entry
{
    int fd;
    int events;
    cwh_event_cb callback;
    void *data;
    struct cwh_event_entry *next;
} cwh_event_entry_t;

// kqueue-based event loop
typedef struct cwh_kqueue
{
    int kqueue_fd;
    struct kevent *events;
    int max_events;
    cwh_event_entry_t *handlers; // Linked list of event handlers
    int running;
    void *loop_ptr; // Pointer back to cwh_loop_t for callbacks
} cwh_kqueue_t;

// Create kqueue instance
cwh_kqueue_t *cwh_kqueue_create(int max_events)
{
    cwh_kqueue_t *kq = (cwh_kqueue_t *)calloc(1, sizeof(cwh_kqueue_t));
    if (!kq)
        return NULL;

    kq->kqueue_fd = kqueue();
    if (kq->kqueue_fd < 0)
    {
        free(kq);
        return NULL;
    }

    kq->events = (struct kevent *)calloc(max_events, sizeof(struct kevent));
    if (!kq->events)
    {
        close(kq->kqueue_fd);
        free(kq);
        return NULL;
    }

    kq->max_events = max_events;
    kq->handlers = NULL;
    kq->running = 0;
    kq->loop_ptr = NULL; // Will be set by loop.c

    return kq;
}

// Find event handler by fd
static cwh_event_entry_t *find_handler(cwh_kqueue_t *kq, int fd)
{
    cwh_event_entry_t *curr = kq->handlers;
    while (curr)
    {
        if (curr->fd == fd)
            return curr;
        curr = curr->next;
    }
    return NULL;
}

// Add file descriptor to kqueue
int cwh_kqueue_add(cwh_kqueue_t *kq, int fd, int events, cwh_event_cb cb, void *data)
{
    if (!kq || fd < 0 || !cb)
        return -1;

    // Check if already registered
    if (find_handler(kq, fd))
        return -1;

    // Create handler entry
    cwh_event_entry_t *entry = (cwh_event_entry_t *)calloc(1, sizeof(cwh_event_entry_t));
    if (!entry)
        return -1;

    entry->fd = fd;
    entry->events = events;
    entry->callback = cb;
    entry->data = data;

    // Add to kqueue using EV_SET
    struct kevent kev[2];
    int nchanges = 0;

    if (events & CWH_EVENT_READ)
    {
        EV_SET(&kev[nchanges], fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL);
        nchanges++;
    }

    if (events & CWH_EVENT_WRITE)
    {
        EV_SET(&kev[nchanges], fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, NULL);
        nchanges++;
    }

    if (nchanges > 0 && kevent(kq->kqueue_fd, kev, nchanges, NULL, 0, NULL) < 0)
    {
        free(entry);
        return -1;
    }

    // Add to handler list
    entry->next = kq->handlers;
    kq->handlers = entry;

    return 0;
}

// Modify file descriptor events
int cwh_kqueue_mod(cwh_kqueue_t *kq, int fd, int events)
{
    if (!kq || fd < 0)
        return -1;

    // Find handler
    cwh_event_entry_t *entry = find_handler(kq, fd);
    if (!entry)
        return -1;

    // Determine changes needed
    int old_events = entry->events;
    struct kevent kev[4];
    int nchanges = 0;

    // Remove old events if needed
    if ((old_events & CWH_EVENT_READ) && !(events & CWH_EVENT_READ))
    {
        EV_SET(&kev[nchanges], fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
        nchanges++;
    }

    if ((old_events & CWH_EVENT_WRITE) && !(events & CWH_EVENT_WRITE))
    {
        EV_SET(&kev[nchanges], fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
        nchanges++;
    }

    // Add new events if needed
    if (!(old_events & CWH_EVENT_READ) && (events & CWH_EVENT_READ))
    {
        EV_SET(&kev[nchanges], fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL);
        nchanges++;
    }

    if (!(old_events & CWH_EVENT_WRITE) && (events & CWH_EVENT_WRITE))
    {
        EV_SET(&kev[nchanges], fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, NULL);
        nchanges++;
    }

    // Apply changes
    if (nchanges > 0 && kevent(kq->kqueue_fd, kev, nchanges, NULL, 0, NULL) < 0)
    {
        return -1;
    }

    // Update events
    entry->events = events;

    return 0;
}

// Remove file descriptor from kqueue
int cwh_kqueue_del(cwh_kqueue_t *kq, int fd)
{
    if (!kq || fd < 0)
        return -1;

    // Find handler
    cwh_event_entry_t *entry = find_handler(kq, fd);
    if (!entry)
        return -1;

    // Remove from kqueue
    struct kevent kev[2];
    int nchanges = 0;

    if (entry->events & CWH_EVENT_READ)
    {
        EV_SET(&kev[nchanges], fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
        nchanges++;
    }

    if (entry->events & CWH_EVENT_WRITE)
    {
        EV_SET(&kev[nchanges], fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
        nchanges++;
    }

    if (nchanges > 0)
    {
        kevent(kq->kqueue_fd, kev, nchanges, NULL, 0, NULL);
        // Ignore errors on delete - fd might already be closed
    }

    // Remove from handler list
    cwh_event_entry_t *prev = NULL;
    cwh_event_entry_t *curr = kq->handlers;
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
                kq->handlers = curr->next;
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
int cwh_kqueue_wait(cwh_kqueue_t *kq, int timeout_ms)
{
    if (!kq)
        return -1;

    // Setup timeout
    struct timespec ts;
    struct timespec *ts_ptr = NULL;
    if (timeout_ms >= 0)
    {
        ts.tv_sec = timeout_ms / 1000;
        ts.tv_nsec = (timeout_ms % 1000) * 1000000;
        ts_ptr = &ts;
    }

    // Call kevent
    int nfds = kevent(kq->kqueue_fd, NULL, 0, kq->events, kq->max_events, ts_ptr);
    if (nfds < 0)
    {
        if (errno == EINTR)
            return 0; // Interrupted, not an error
        return -1;
    }

    // Dispatch callbacks
    for (int i = 0; i < nfds; i++)
    {
        int fd = (int)kq->events[i].ident;
        cwh_event_entry_t *entry = find_handler(kq, fd);
        if (entry && entry->callback)
        {
            int events = 0;

            // Convert kqueue events to cwebhttp events
            if (kq->events[i].filter == EVFILT_READ)
                events |= CWH_EVENT_READ;
            if (kq->events[i].filter == EVFILT_WRITE)
                events |= CWH_EVENT_WRITE;
            if (kq->events[i].flags & EV_ERROR)
                events |= CWH_EVENT_ERROR;
            if (kq->events[i].flags & EV_EOF)
                events |= CWH_EVENT_ERROR;

            entry->callback((cwh_loop_t *)kq->loop_ptr, fd, events, entry->data);
        }
    }

    return nfds;
}

// Stop kqueue loop
void cwh_kqueue_stop(cwh_kqueue_t *kq)
{
    if (kq)
    {
        kq->running = 0;
    }
}

// Run kqueue loop
int cwh_kqueue_run(cwh_kqueue_t *kq)
{
    if (!kq)
        return -1;

    kq->running = 1;
    while (kq->running)
    {
        int ret = cwh_kqueue_wait(kq, -1); // Block indefinitely
        if (ret < 0)
            return -1;
    }

    return 0;
}

// Cleanup kqueue
void cwh_kqueue_free(cwh_kqueue_t *kq)
{
    if (!kq)
        return;

    // Free handler list
    cwh_event_entry_t *curr = kq->handlers;
    while (curr)
    {
        cwh_event_entry_t *next = curr->next;
        free(curr);
        curr = next;
    }

    // Close kqueue fd
    if (kq->kqueue_fd >= 0)
    {
        close(kq->kqueue_fd);
    }

    // Free events array
    free(kq->events);
    free(kq);
}

// Set loop pointer (called by loop.c after creation)
void cwh_kqueue_set_loop(cwh_kqueue_t *kq, void *loop)
{
    if (kq)
    {
        kq->loop_ptr = loop;
    }
}

// Get backend name
const char *cwh_kqueue_backend(void)
{
    return "kqueue (macOS/BSD)";
}

#endif // __APPLE__ || __FreeBSD__ || __OpenBSD__ || __NetBSD__
