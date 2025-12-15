// select.c - Portable select backend for async event loop
// Universal fallback for platforms without epoll/kqueue/IOCP

#include "../../include/cwebhttp_async.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#ifndef _WIN32
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#else
#include <winsock2.h>
#endif

// Event handler entry
typedef struct cwh_event_entry
{
    int fd;
    int events;
    cwh_event_cb callback;
    void *data;
    struct cwh_event_entry *next;
} cwh_event_entry_t;

// select-based event loop
typedef struct cwh_select
{
    cwh_event_entry_t *handlers; // Linked list of event handlers
    int max_fd;                  // Highest file descriptor
    int running;                 // Loop running flag
    void *loop_ptr;              // Pointer back to cwh_loop_t for callbacks
} cwh_select_t;

// Create select instance
cwh_select_t *cwh_select_create(void)
{
    cwh_select_t *sel = (cwh_select_t *)calloc(1, sizeof(cwh_select_t));
    if (!sel)
        return NULL;

    sel->handlers = NULL;
    sel->max_fd = -1;
    sel->running = 0;
    sel->loop_ptr = NULL;

    return sel;
}

// Find event handler by fd
static cwh_event_entry_t *find_handler(cwh_select_t *sel, int fd)
{
    cwh_event_entry_t *curr = sel->handlers;
    while (curr)
    {
        if (curr->fd == fd)
            return curr;
        curr = curr->next;
    }
    return NULL;
}

// Update max_fd
static void update_max_fd(cwh_select_t *sel)
{
    sel->max_fd = -1;
    cwh_event_entry_t *curr = sel->handlers;
    while (curr)
    {
        if (curr->fd > sel->max_fd)
            sel->max_fd = curr->fd;
        curr = curr->next;
    }
}

// Add file descriptor to select
int cwh_select_add(cwh_select_t *sel, int fd, int events, cwh_event_cb cb, void *data)
{
    if (!sel || fd < 0 || !cb)
        return -1;

    // Check if already registered
    if (find_handler(sel, fd))
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
    entry->next = sel->handlers;
    sel->handlers = entry;

    // Update max_fd
    if (fd > sel->max_fd)
        sel->max_fd = fd;

    printf("[DEBUG] cwh_select_add: fd=%d, events=%d, max_fd=%d\n", fd, events, sel->max_fd);

    return 0;
}

// Modify file descriptor events
int cwh_select_mod(cwh_select_t *sel, int fd, int events)
{
    if (!sel || fd < 0)
        return -1;

    // Find handler
    cwh_event_entry_t *entry = find_handler(sel, fd);
    if (!entry)
        return -1;

    // Update events
    entry->events = events;

    return 0;
}

// Remove file descriptor from select
int cwh_select_del(cwh_select_t *sel, int fd)
{
    if (!sel || fd < 0)
        return -1;

    // Remove from handler list
    cwh_event_entry_t *prev = NULL;
    cwh_event_entry_t *curr = sel->handlers;
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
                sel->handlers = curr->next;
            }
            free(curr);

            // Update max_fd
            update_max_fd(sel);

            return 0;
        }
        prev = curr;
        curr = curr->next;
    }

    return -1;
}

// Wait for events and dispatch callbacks
int cwh_select_wait(cwh_select_t *sel, int timeout_ms)
{
    if (!sel)
        return -1;

    printf("[DEBUG] cwh_select_wait: max_fd=%d\n", sel->max_fd);

    // No handlers registered
    if (sel->max_fd < 0)
    {
        printf("[DEBUG] No handlers registered, returning\n");
        return 0;
    }

    // Build fd_sets
    fd_set read_fds, write_fds, error_fds;
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    FD_ZERO(&error_fds);

    cwh_event_entry_t *curr = sel->handlers;
    while (curr)
    {
        if (curr->events & CWH_EVENT_READ)
            FD_SET(curr->fd, &read_fds);
        if (curr->events & CWH_EVENT_WRITE)
            FD_SET(curr->fd, &write_fds);
        FD_SET(curr->fd, &error_fds); // Always monitor errors

        curr = curr->next;
    }

    // Setup timeout
    struct timeval tv;
    struct timeval *tv_ptr = NULL;
    if (timeout_ms >= 0)
    {
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        tv_ptr = &tv;
    }

    // Call select
    printf("[DEBUG] Calling select() with max_fd=%d\n", sel->max_fd);
    int ret = select(sel->max_fd + 1, &read_fds, &write_fds, &error_fds, tv_ptr);
    printf("[DEBUG] select() returned: %d\n", ret);

    if (ret < 0)
    {
#ifdef _WIN32
        if (WSAGetLastError() == WSAEINTR)
            return 0; // Interrupted, not an error
#else
        if (errno == EINTR)
            return 0; // Interrupted, not an error
#endif
        return -1;
    }

    if (ret == 0)
        return 0; // Timeout

    // Dispatch callbacks
    int event_count = 0;
    curr = sel->handlers;
    while (curr && event_count < ret)
    {
        int events = 0;

        if (FD_ISSET(curr->fd, &read_fds))
            events |= CWH_EVENT_READ;
        if (FD_ISSET(curr->fd, &write_fds))
            events |= CWH_EVENT_WRITE;
        if (FD_ISSET(curr->fd, &error_fds))
            events |= CWH_EVENT_ERROR;

        if (events && curr->callback)
        {
            curr->callback((cwh_loop_t *)sel->loop_ptr, curr->fd, events, curr->data);
            event_count++;
        }

        curr = curr->next;
    }

    return event_count;
}

// Stop select loop
void cwh_select_stop(cwh_select_t *sel)
{
    if (sel)
    {
        sel->running = 0;
    }
}

// Run select loop
int cwh_select_run(cwh_select_t *sel)
{
    if (!sel)
        return -1;

    sel->running = 1;
    while (sel->running)
    {
        int ret = cwh_select_wait(sel, -1); // Block indefinitely
        if (ret < 0)
            return -1;
    }

    return 0;
}

// Cleanup select
void cwh_select_free(cwh_select_t *sel)
{
    if (!sel)
        return;

    // Free handler list
    cwh_event_entry_t *curr = sel->handlers;
    while (curr)
    {
        cwh_event_entry_t *next = curr->next;
        free(curr);
        curr = next;
    }

    free(sel);
}

// Set loop pointer (called by loop.c after creation)
void cwh_select_set_loop(cwh_select_t *sel, void *loop)
{
    if (sel)
    {
        sel->loop_ptr = loop;
    }
}

// Get backend name
const char *cwh_select_backend(void)
{
    return "select (portable)";
}
