// wsapoll.c - Windows WSAPoll backend for async event loop
// Modern Windows polling API without FD_SETSIZE limitations

#include "../../include/cwebhttp_async.h"
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>

// Event handler entry
typedef struct cwh_event_entry
{
    int fd;
    int events;
    cwh_event_cb callback;
    void *data;
    struct cwh_event_entry *next;
} cwh_event_entry_t;

// WSAPoll-based event loop
typedef struct cwh_wsapoll
{
    cwh_event_entry_t *handlers; // Linked list of event handlers
    WSAPOLLFD *pollfds;          // Dynamic array of poll structures
    int nfds;                    // Number of active fds
    int capacity;                // Allocated capacity
    int running;                 // Loop running flag
    void *loop_ptr;              // Pointer back to cwh_loop_t for callbacks
} cwh_wsapoll_t;

// Create WSAPoll instance
cwh_wsapoll_t *cwh_wsapoll_create(void)
{
    cwh_wsapoll_t *poll = (cwh_wsapoll_t *)calloc(1, sizeof(cwh_wsapoll_t));
    if (!poll)
        return NULL;

    poll->handlers = NULL;
    poll->pollfds = NULL;
    poll->nfds = 0;
    poll->capacity = 0;
    poll->running = 0;
    poll->loop_ptr = NULL;

    return poll;
}

// Set loop pointer (for callbacks)
void cwh_wsapoll_set_loop(cwh_wsapoll_t *poll, void *loop)
{
    if (poll)
    {
        poll->loop_ptr = loop;
    }
}

// Find event handler by fd
static cwh_event_entry_t *find_handler(cwh_wsapoll_t *poll, int fd)
{
    cwh_event_entry_t *curr = poll->handlers;
    while (curr)
    {
        if (curr->fd == fd)
            return curr;
        curr = curr->next;
    }
    return NULL;
}

// Rebuild pollfds array from handler list
static int rebuild_pollfds(cwh_wsapoll_t *poll)
{
    // Count handlers
    int count = 0;
    cwh_event_entry_t *curr = poll->handlers;
    while (curr)
    {
        count++;
        curr = curr->next;
    }

    // Reallocate if needed
    if (count > poll->capacity)
    {
        int new_capacity = count + 16; // Add some headroom
        WSAPOLLFD *new_fds = (WSAPOLLFD *)realloc(poll->pollfds,
                                                  new_capacity * sizeof(WSAPOLLFD));
        if (!new_fds)
            return -1;
        poll->pollfds = new_fds;
        poll->capacity = new_capacity;
    }

    // Build pollfd array
    poll->nfds = 0;
    curr = poll->handlers;
    while (curr)
    {
        WSAPOLLFD *pfd = &poll->pollfds[poll->nfds];
        pfd->fd = (SOCKET)curr->fd;
        pfd->events = 0;
        pfd->revents = 0;

        if (curr->events & CWH_EVENT_READ)
            pfd->events |= POLLIN;
        if (curr->events & CWH_EVENT_WRITE)
            pfd->events |= POLLOUT;

        poll->nfds++;
        curr = curr->next;
    }

    return 0;
}

// Add file descriptor to WSAPoll
int cwh_wsapoll_add(cwh_wsapoll_t *poll, int fd, int events, cwh_event_cb cb, void *data)
{
    if (!poll || fd < 0 || !cb)
        return -1;

    // Check if already registered
    if (find_handler(poll, fd))
        return -1;

    // Create handler entry
    cwh_event_entry_t *entry = (cwh_event_entry_t *)calloc(1, sizeof(cwh_event_entry_t));
    if (!entry)
        return -1;

    entry->fd = fd;
    entry->events = events;
    entry->callback = cb;
    entry->data = data;

    // Add to handler list
    entry->next = poll->handlers;
    poll->handlers = entry;

    // Rebuild pollfds array
    if (rebuild_pollfds(poll) < 0)
    {
        // Remove from list
        poll->handlers = entry->next;
        free(entry);
        return -1;
    }

    return 0;
}

// Modify file descriptor events
int cwh_wsapoll_mod(cwh_wsapoll_t *poll, int fd, int events)
{
    if (!poll || fd < 0)
        return -1;

    // Find handler
    cwh_event_entry_t *entry = find_handler(poll, fd);
    if (!entry)
        return -1;

    // Update events
    entry->events = events;

    // Rebuild pollfds array
    return rebuild_pollfds(poll);
}

// Remove file descriptor from WSAPoll
int cwh_wsapoll_del(cwh_wsapoll_t *poll, int fd)
{
    if (!poll || fd < 0)
        return -1;

    // Find and remove from handler list
    cwh_event_entry_t **prev_ptr = &poll->handlers;
    cwh_event_entry_t *curr = poll->handlers;

    while (curr)
    {
        if (curr->fd == fd)
        {
            *prev_ptr = curr->next;
            free(curr);

            // Rebuild pollfds array
            return rebuild_pollfds(poll);
        }
        prev_ptr = &curr->next;
        curr = curr->next;
    }

    return -1; // Not found
}

// Wait for events and dispatch callbacks
int cwh_wsapoll_wait(cwh_wsapoll_t *poll, int timeout_ms)
{
    if (!poll)
        return -1;

    // No handlers registered
    if (poll->nfds == 0)
        return 0;

    // Call WSAPoll
    int ret = WSAPoll(poll->pollfds, poll->nfds, timeout_ms);

    if (ret < 0)
    {
        if (WSAGetLastError() == WSAEINTR)
            return 0; // Interrupted, not an error
        return -1;
    }

    if (ret == 0)
        return 0; // Timeout

    // Dispatch callbacks
    int event_count = 0;
    for (int i = 0; i < poll->nfds && event_count < ret; i++)
    {
        WSAPOLLFD *pfd = &poll->pollfds[i];
        if (pfd->revents == 0)
            continue;

        // Find handler for this fd
        cwh_event_entry_t *curr = poll->handlers;
        while (curr)
        {
            if (curr->fd == (int)pfd->fd)
            {
                int events = 0;

                if (pfd->revents & POLLIN)
                    events |= CWH_EVENT_READ;
                if (pfd->revents & POLLOUT)
                    events |= CWH_EVENT_WRITE;
                if (pfd->revents & (POLLERR | POLLHUP | POLLNVAL))
                    events |= CWH_EVENT_ERROR;

                if (events && curr->callback)
                {
                    curr->callback((cwh_loop_t *)poll->loop_ptr, curr->fd, events, curr->data);
                    event_count++;
                }
                break;
            }
            curr = curr->next;
        }
    }

    return event_count;
}

// Stop WSAPoll loop
void cwh_wsapoll_stop(cwh_wsapoll_t *poll)
{
    if (poll)
    {
        poll->running = 0;
    }
}

// Run WSAPoll loop
int cwh_wsapoll_run(cwh_wsapoll_t *poll)
{
    if (!poll)
        return -1;

    poll->running = 1;
    while (poll->running)
    {
        int ret = cwh_wsapoll_wait(poll, -1); // Block indefinitely
        if (ret < 0)
            return -1;
    }

    return 0;
}

// Cleanup WSAPoll
void cwh_wsapoll_free(cwh_wsapoll_t *poll)
{
    if (!poll)
        return;

    // Free handler list
    cwh_event_entry_t *curr = poll->handlers;
    while (curr)
    {
        cwh_event_entry_t *next = curr->next;
        free(curr);
        curr = next;
    }

    // Free pollfds array
    if (poll->pollfds)
        free(poll->pollfds);

    free(poll);
}

#endif // _WIN32
