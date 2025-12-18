// iocp.c - Windows IOCP backend for async event loop
// High-performance completion-based I/O for Windows

#include "../../include/cwebhttp_async.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <windows.h>
#include <mswsock.h>

// AcceptEx function pointer (loaded at runtime)
static LPFN_ACCEPTEX AcceptExPtr = NULL;
static LPFN_GETACCEPTEXSOCKADDRS GetAcceptExSockaddrsPtr = NULL;

// GUID for AcceptEx
static GUID GuidAcceptEx = WSAID_ACCEPTEX;
static GUID GuidGetAcceptExSockaddrs = WSAID_GETACCEPTEXSOCKADDRS;

// Event handler entry
typedef struct cwh_event_entry
{
    int fd;
    int events;
    cwh_event_cb callback;
    void *data;
    struct cwh_event_entry *next;

    // IOCP-specific: overlapped structures for async operations
    OVERLAPPED read_overlapped;
    OVERLAPPED write_overlapped;
    OVERLAPPED accept_overlapped;
    char read_buffer[4096];
    char accept_buffer[512]; // Buffer for AcceptEx (local and remote addresses)
    SOCKET accept_socket;    // Pre-created socket for AcceptEx
    int read_pending;
    int write_pending;
    int accept_pending;
    int is_listen_socket; // Flag to identify listen sockets
    HANDLE iocp_handle;   // Back-reference to IOCP handle for AcceptEx
    DWORD bytes_received; // Bytes received in last read completion (IOCP only)
} cwh_event_entry_t;

// IOCP-based event loop
typedef struct cwh_iocp
{
    HANDLE iocp_handle;
    cwh_event_entry_t *handlers; // Linked list of event handlers
    int running;
    void *loop_ptr; // Pointer back to cwh_loop_t for callbacks
} cwh_iocp_t;

// Load AcceptEx function pointers at runtime
static int load_acceptex(SOCKET sock)
{
    if (AcceptExPtr != NULL)
        return 0; // Already loaded

    DWORD bytes = 0;

    // Load AcceptEx
    int ret = WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER,
                       &GuidAcceptEx, sizeof(GuidAcceptEx),
                       &AcceptExPtr, sizeof(AcceptExPtr),
                       &bytes, NULL, NULL);
    if (ret != 0)
        return -1;

    // Load GetAcceptExSockaddrs
    ret = WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER,
                   &GuidGetAcceptExSockaddrs, sizeof(GuidGetAcceptExSockaddrs),
                   &GetAcceptExSockaddrsPtr, sizeof(GetAcceptExSockaddrsPtr),
                   &bytes, NULL, NULL);
    if (ret != 0)
        return -1;

    return 0;
}

// Create IOCP instance
cwh_iocp_t *cwh_iocp_create(void)
{
    cwh_iocp_t *iocp = (cwh_iocp_t *)calloc(1, sizeof(cwh_iocp_t));
    if (!iocp)
        return NULL;

    // Create IOCP handle with unlimited threads (0 = number of CPUs)
    iocp->iocp_handle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (!iocp->iocp_handle)
    {
        free(iocp);
        return NULL;
    }

    iocp->handlers = NULL;
    iocp->running = 0;
    iocp->loop_ptr = NULL;

    return iocp;
}

// Forward declaration
static int post_acceptex(cwh_event_entry_t *entry);

// Find event handler by fd
static cwh_event_entry_t *find_handler(cwh_iocp_t *iocp, int fd)
{
    cwh_event_entry_t *curr = iocp->handlers;
    while (curr)
    {
        if (curr->fd == fd)
            return curr;
        curr = curr->next;
    }
    return NULL;
}

// Retrieve the accepted socket from a listen socket entry (after AcceptEx completion)
// Returns the accepted socket and clears it, or INVALID_SOCKET if none available
static SOCKET retrieve_accepted_socket(cwh_iocp_t *iocp, int listen_fd)
{
    cwh_event_entry_t *entry = find_handler(iocp, listen_fd);
    if (!entry || !entry->is_listen_socket)
        return INVALID_SOCKET;

    SOCKET accepted = entry->accept_socket;
    entry->accept_socket = INVALID_SOCKET;

    // Post another AcceptEx to continue accepting connections
    if (accepted != INVALID_SOCKET && (entry->events & CWH_EVENT_READ))
    {
        post_acceptex(entry);
    }

    return accepted;
}

// Post an AcceptEx operation for a listen socket
static int post_acceptex(cwh_event_entry_t *entry)
{
    printf("[IOCP] post_acceptex: entry=%p, is_listen=%d, pending=%d\n",
           entry, entry ? entry->is_listen_socket : -1, entry ? entry->accept_pending : -1);

    if (!entry || !entry->is_listen_socket || entry->accept_pending)
    {
        printf("[IOCP] post_acceptex: Early return - invalid conditions\n");
        return -1;
    }

    // Check if AcceptEx is loaded
    if (!AcceptExPtr)
    {
        printf("[IOCP] post_acceptex: ERROR - AcceptExPtr is NULL!\n");
        return -1;
    }

    printf("[IOCP] post_acceptex: AcceptExPtr loaded, proceeding...\n");

    // Create accept socket if needed
    if (entry->accept_socket == INVALID_SOCKET)
    {
        // Get socket info to create compatible socket
        WSAPROTOCOL_INFO protocol_info;
        int len = sizeof(protocol_info);
        if (getsockopt((SOCKET)entry->fd, SOL_SOCKET, SO_PROTOCOL_INFO,
                       (char *)&protocol_info, &len) != 0)
        {
            // Fallback: create AF_INET TCP socket with WSA_FLAG_OVERLAPPED
            entry->accept_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP,
                                             NULL, 0, WSA_FLAG_OVERLAPPED);
        }
        else
        {
            // Create socket with overlapped flag (critical for IOCP)
            entry->accept_socket = WSASocket(protocol_info.iAddressFamily,
                                             protocol_info.iSocketType,
                                             protocol_info.iProtocol,
                                             NULL, 0, WSA_FLAG_OVERLAPPED);
        }

        if (entry->accept_socket == INVALID_SOCKET)
            return -1;

        // DO NOT associate the accept socket here!
        // AcceptEx completions will be delivered to the LISTEN socket's completion port
        // The accept socket will be associated later when the connection is created
        printf("[IOCP] Accept socket created: %d (will be associated later)\n",
               (int)entry->accept_socket);
    }

    // Reset overlapped structure completely
    memset(&entry->accept_overlapped, 0, sizeof(OVERLAPPED));

    // AcceptEx requires buffer space for:
    // - dwReceiveDataLength bytes for initial data (we use 0 for immediate completion)
    // - dwLocalAddressLength bytes for local address + padding
    // - dwRemoteAddressLength bytes for remote address + padding
    //
    // Microsoft docs: "The address data is written in an internal format.
    // To convert the sockaddr structures, use GetAcceptExSockaddrs."
    // Each address needs sizeof(sockaddr_in) + 16 bytes minimum
    DWORD addr_len = sizeof(struct sockaddr_in) + 16;

    // CRITICAL FIX: Use 0 for dwReceiveDataLength to make AcceptEx complete
    // immediately upon connection (not wait for client data).
    // The buffer still needs space for both addresses.
    DWORD receive_size = 0;

    // Post AcceptEx operation
    DWORD bytes_received = 0;

    printf("[IOCP] Calling AcceptEx: listen_fd=%d, accept_socket=%d\n",
           (int)entry->fd, (int)entry->accept_socket);

    BOOL ret = AcceptExPtr((SOCKET)entry->fd,
                           entry->accept_socket,
                           entry->accept_buffer,
                           receive_size, // 0 = complete on connection, don't wait for data
                           addr_len,     // Local address + 16 bytes
                           addr_len,     // Remote address + 16 bytes
                           &bytes_received,
                           &entry->accept_overlapped);

    DWORD error = WSAGetLastError();

    printf("[IOCP] AcceptEx returned: ret=%d, error=%lu, bytes=%lu\n",
           ret, error, bytes_received);

    // AcceptEx returns FALSE with WSA_IO_PENDING for async operation
    // or TRUE if it completed synchronously
    if (!ret && error != WSA_IO_PENDING)
    {
        // AcceptEx failed
        closesocket(entry->accept_socket);
        entry->accept_socket = INVALID_SOCKET;
        return -1;
    }

    // If AcceptEx completed synchronously (ret == TRUE), we still need to
    // post a completion to IOCP manually or handle it here
    if (ret)
    {
        printf("[IOCP] AcceptEx completed SYNCHRONOUSLY, posting completion manually\n");
        // Synchronous completion - post completion packet manually
        PostQueuedCompletionStatus(entry->iocp_handle,
                                   bytes_received,
                                   (ULONG_PTR)entry,
                                   &entry->accept_overlapped);
    }
    else if (error == WSA_IO_PENDING)
    {
        printf("[IOCP] AcceptEx pending (async), waiting for completion...\n");
    }

    entry->accept_pending = 1;
    return 0;
}

// Add file descriptor to IOCP
int cwh_iocp_add(cwh_iocp_t *iocp, int fd, int events, cwh_event_cb cb, void *data)
{
    if (!iocp || fd < 0 || !cb)
        return -1;

    // Check if already registered
    if (find_handler(iocp, fd))
        return -1;

    // Create handler entry
    cwh_event_entry_t *entry = (cwh_event_entry_t *)calloc(1, sizeof(cwh_event_entry_t));
    if (!entry)
        return -1;

    entry->fd = fd;
    entry->events = events;
    entry->callback = cb;
    entry->data = data;
    entry->read_pending = 0;
    entry->write_pending = 0;
    entry->accept_pending = 0;
    entry->accept_socket = INVALID_SOCKET;
    entry->is_listen_socket = 0;
    entry->iocp_handle = iocp->iocp_handle; // Store IOCP handle for AcceptEx

    // Initialize overlapped structures
    memset(&entry->read_overlapped, 0, sizeof(OVERLAPPED));
    memset(&entry->write_overlapped, 0, sizeof(OVERLAPPED));
    memset(&entry->accept_overlapped, 0, sizeof(OVERLAPPED));

    // Detect if this is a listen socket
    BOOL accept_conn = FALSE;
    int optlen = sizeof(accept_conn);
    if (getsockopt((SOCKET)fd, SOL_SOCKET, SO_ACCEPTCONN,
                   (char *)&accept_conn, &optlen) == 0 &&
        accept_conn)
    {
        entry->is_listen_socket = 1;

        // Load AcceptEx function pointers
        if (load_acceptex((SOCKET)fd) != 0)
        {
            free(entry);
            return -1;
        }
    }

    // Associate socket with IOCP
    // CRITICAL: For AcceptEx to work with MinGW, we MUST associate the listen socket!
    // For AcceptEx'd client sockets, CreateIoCompletionPort will UPDATE the completion key
    // from the listen socket's key to this new entry's key
    printf("[IOCP] Associating socket fd=%d (is_listen=%d) with IOCP handle=%p, key=%p\n",
           fd, entry->is_listen_socket, iocp->iocp_handle, entry);

    HANDLE result = CreateIoCompletionPort((HANDLE)(uintptr_t)fd, iocp->iocp_handle, (ULONG_PTR)entry, 0);
    if (!result)
    {
        DWORD err = GetLastError();
        printf("[IOCP] ERROR: Failed to associate socket with IOCP, error=%lu\n", err);
        free(entry);
        return -1;
    }

    printf("[IOCP] Socket fd=%d successfully associated/re-associated with IOCP (result=%p)\n", fd, result);

    // Add to handler list
    entry->next = iocp->handlers;
    iocp->handlers = entry;

    // For IOCP, we need to start async operations immediately
    // This is different from epoll/kqueue which are readiness-based
    printf("[IOCP] cwh_iocp_add: fd=%d, events=%d, is_listen=%d\n", fd, events, entry->is_listen_socket);

    if (events & CWH_EVENT_READ)
    {
        if (entry->is_listen_socket)
        {
            printf("[IOCP] Detected listen socket, calling post_acceptex...\n");
            // Post AcceptEx for listen sockets
            int ret = post_acceptex(entry);
            printf("[IOCP] post_acceptex returned: %d\n", ret);
            if (ret != 0)
            {
                printf("[IOCP] WARNING: post_acceptex failed!\n");
                // Failed to post accept, but don't fail the add operation
                // The socket is still registered with IOCP
            }
        }
        else
        {
            printf("[IOCP] Client socket, posting initial read operation...\n");
            // Post a read operation for client sockets
            WSABUF buf;
            buf.buf = entry->read_buffer;
            buf.len = sizeof(entry->read_buffer);

            DWORD flags = 0;
            DWORD bytes_received = 0;

            memset(&entry->read_overlapped, 0, sizeof(OVERLAPPED));

            int ret = WSARecv((SOCKET)fd, &buf, 1, &bytes_received, &flags,
                              &entry->read_overlapped, NULL);

            if (ret == 0 || WSAGetLastError() == WSA_IO_PENDING)
            {
                entry->read_pending = 1;
                printf("[IOCP] Initial read posted successfully (ret=%d, error=%u)\n",
                       ret, WSAGetLastError());
            }
            else
            {
                printf("[IOCP] WARNING: Failed to post initial read (error=%u)\n",
                       WSAGetLastError());
            }
        }
    }

    return 0;
}

// Modify file descriptor events
int cwh_iocp_mod(cwh_iocp_t *iocp, int fd, int events)
{
    if (!iocp || fd < 0)
        return -1;

    // Find handler
    cwh_event_entry_t *entry = find_handler(iocp, fd);
    if (!entry)
        return -1;

    // Update events
    int old_events = entry->events;
    entry->events = events;

    printf("[IOCP] cwh_iocp_mod: fd=%d, old_events=%d, new_events=%d\n", fd, old_events, events);

    // Start read if needed and not already pending
    if ((events & CWH_EVENT_READ) && !entry->read_pending)
    {
        WSABUF buf;
        buf.buf = entry->read_buffer;
        buf.len = sizeof(entry->read_buffer);

        DWORD flags = 0;
        DWORD bytes_received = 0;

        int ret = WSARecv((SOCKET)fd, &buf, 1, &bytes_received, &flags,
                          &entry->read_overlapped, NULL);

        if (ret == 0 || WSAGetLastError() == WSA_IO_PENDING)
        {
            entry->read_pending = 1;
        }
    }

    // Start write if needed and not already pending
    if ((events & CWH_EVENT_WRITE) && !entry->write_pending)
    {
        printf("[IOCP] WRITE event requested, triggering immediate callback\n");

        // For IOCP write, we immediately call the callback which will call write_response()
        // write_response() will then post WSASend
        // This is different from read where we pre-post the operation

        // Immediately trigger WRITE event by calling the callback
        if (entry->callback)
        {
            entry->callback((cwh_loop_t *)iocp->loop_ptr, fd, CWH_EVENT_WRITE, entry->data);
        }
    }

    return 0;
}

// Remove file descriptor from IOCP
int cwh_iocp_del(cwh_iocp_t *iocp, int fd)
{
    if (!iocp || fd < 0)
        return -1;

    // Remove from handler list
    cwh_event_entry_t *prev = NULL;
    cwh_event_entry_t *curr = iocp->handlers;
    while (curr)
    {
        if (curr->fd == fd)
        {
            // Cancel any pending I/O
            CancelIo((HANDLE)(uintptr_t)fd);

            if (prev)
            {
                prev->next = curr->next;
            }
            else
            {
                iocp->handlers = curr->next;
            }
            free(curr);
            return 0;
        }
        prev = curr;
        curr = curr->next;
    }

    return -1;
}

// Wait for completions and dispatch callbacks
int cwh_iocp_wait(cwh_iocp_t *iocp, int timeout_ms)
{
    if (!iocp)
        return -1;

    DWORD bytes_transferred = 0;
    ULONG_PTR completion_key = 0;
    LPOVERLAPPED overlapped = NULL;

    // Wait for completion
    printf("[IOCP] Calling GetQueuedCompletionStatus (timeout=%d)...\n", timeout_ms);

    BOOL result = GetQueuedCompletionStatus(
        iocp->iocp_handle,
        &bytes_transferred,
        &completion_key,
        &overlapped,
        timeout_ms >= 0 ? (DWORD)timeout_ms : INFINITE);

    printf("[IOCP] GetQueuedCompletionStatus returned: result=%d, bytes=%lu, key=%p, overlapped=%p\n",
           result, bytes_transferred, (void *)completion_key, overlapped);

    if (!result && overlapped == NULL)
    {
        // Timeout or error
        DWORD error = GetLastError();
        if (error == WAIT_TIMEOUT)
            return 0;
        return -1;
    }

    if (overlapped == NULL)
    {
        return 0; // Timeout
    }

    // Get the handler entry from completion key
    cwh_event_entry_t *entry = (cwh_event_entry_t *)completion_key;
    if (!entry || !entry->callback)
        return 0;

    // Determine which operation completed
    int events = 0;

    if (overlapped == &entry->accept_overlapped)
    {
        // AcceptEx completed
        entry->accept_pending = 0;

        if (result)
        {
            // Accept succeeded
            // Update accept socket context (required for AcceptEx)
            setsockopt(entry->accept_socket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
                       (char *)&entry->fd, sizeof(entry->fd));

            // Trigger READ event so server can call accept()
            // The accepted socket is stored in entry->accept_socket and will be
            // returned by the next accept() call through a custom Windows-specific path
            events |= CWH_EVENT_READ;

            // DO NOT post another AcceptEx here - let the server's accept() loop
            // handle calling accept() which will trigger posting the next AcceptEx
            // This maintains compatibility with the existing server code
        }
        else
        {
            // Accept failed or was cancelled
            if (entry->accept_socket != INVALID_SOCKET)
            {
                closesocket(entry->accept_socket);
                entry->accept_socket = INVALID_SOCKET;
            }

            // Post another accept if still interested (retry on error)
            if (entry->events & CWH_EVENT_READ)
            {
                post_acceptex(entry);
            }
        }
    }
    else if (overlapped == &entry->read_overlapped)
    {
        entry->read_pending = 0;

        if (result)
        {
            // Store how many bytes were received
            entry->bytes_received = bytes_transferred;
            printf("[IOCP] Read completion: %lu bytes received\n", bytes_transferred);

            events |= CWH_EVENT_READ;

            // Re-post read if still interested
            if (entry->events & CWH_EVENT_READ)
            {
                WSABUF buf;
                buf.buf = entry->read_buffer;
                buf.len = sizeof(entry->read_buffer);

                DWORD flags = 0;
                DWORD bytes_received = 0;

                int ret = WSARecv((SOCKET)entry->fd, &buf, 1, &bytes_received, &flags,
                                  &entry->read_overlapped, NULL);

                if (ret == 0 || WSAGetLastError() == WSA_IO_PENDING)
                {
                    entry->read_pending = 1;
                }
            }
        }
        else
        {
            events |= CWH_EVENT_ERROR;
        }
    }
    else if (overlapped == &entry->write_overlapped)
    {
        entry->write_pending = 0;

        if (result)
        {
            events |= CWH_EVENT_WRITE;
        }
        else
        {
            events |= CWH_EVENT_ERROR;
        }
    }

    // Dispatch callback
    if (events)
    {
        entry->callback((cwh_loop_t *)iocp->loop_ptr, entry->fd, events, entry->data);
    }

    return 1;
}

// Stop IOCP loop
void cwh_iocp_stop(cwh_iocp_t *iocp)
{
    if (iocp)
    {
        iocp->running = 0;

        // Post a special completion to wake up waiting threads
        PostQueuedCompletionStatus(iocp->iocp_handle, 0, 0, NULL);
    }
}

// Run IOCP loop
int cwh_iocp_run(cwh_iocp_t *iocp)
{
    if (!iocp)
        return -1;

    iocp->running = 1;
    while (iocp->running)
    {
        int ret = cwh_iocp_wait(iocp, -1); // Block indefinitely
        if (ret < 0)
            return -1;
    }

    return 0;
}

// Get buffered received data (IOCP already read it via WSARecv)
int cwh_iocp_get_received_data(cwh_iocp_t *iocp, int fd, char *buffer, int size)
{
    if (!iocp || fd < 0 || !buffer || size <= 0)
        return 0;

    cwh_event_entry_t *entry = find_handler(iocp, fd);
    if (!entry)
        return 0;

    // Check if there's buffered data from last read completion
    if (entry->bytes_received > 0 && entry->bytes_received <= sizeof(entry->read_buffer))
    {
        int copy_size = (int)entry->bytes_received;
        if (copy_size > size)
            copy_size = size;

        memcpy(buffer, entry->read_buffer, copy_size);

        printf("[IOCP] Returning %d buffered bytes to application\n", copy_size);

        // Clear the buffered data
        entry->bytes_received = 0;

        return copy_size;
    }

    return 0;
}

// Cleanup IOCP
void cwh_iocp_free(cwh_iocp_t *iocp)
{
    if (!iocp)
        return;

    // Free handler list
    cwh_event_entry_t *curr = iocp->handlers;
    while (curr)
    {
        cwh_event_entry_t *next = curr->next;

        // Cancel any pending I/O
        CancelIo((HANDLE)(uintptr_t)curr->fd);

        free(curr);
        curr = next;
    }

    // Close IOCP handle
    if (iocp->iocp_handle)
    {
        CloseHandle(iocp->iocp_handle);
    }

    free(iocp);
}

// Set loop pointer (called by loop.c after creation)
void cwh_iocp_set_loop(cwh_iocp_t *iocp, void *loop)
{
    if (iocp)
    {
        iocp->loop_ptr = loop;
    }
}

// Get backend name
const char *cwh_iocp_backend(void)
{
    return "IOCP (Windows)";
}

// Get accepted socket from IOCP for a listen socket (AcceptEx integration)
// This is called by accept() wrapper on Windows to retrieve AcceptEx'd sockets
int cwh_iocp_get_accepted_socket(cwh_iocp_t *iocp, int listen_fd)
{
    if (!iocp)
        return -1;

    SOCKET sock = retrieve_accepted_socket(iocp, listen_fd);
    return (sock == INVALID_SOCKET) ? -1 : (int)sock;
}

#endif // _WIN32 || _WIN64
