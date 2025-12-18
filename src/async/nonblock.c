// nonblock.c - Non-blocking socket utilities
// Cross-platform functions to set socket blocking mode

#include "../../include/cwebhttp_async.h"

#ifdef _WIN32
#include <winsock2.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

// Set socket to non-blocking mode
int cwh_set_nonblocking(int fd)
{
    if (fd < 0)
        return -1;

#ifdef _WIN32
    unsigned long mode = 1; // 1 = non-blocking
    return ioctlsocket(fd, FIONBIO, &mode);
#else
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#endif
}

// Set socket to blocking mode
int cwh_set_blocking(int fd)
{
    if (fd < 0)
        return -1;

#ifdef _WIN32
    unsigned long mode = 0; // 0 = blocking
    return ioctlsocket(fd, FIONBIO, &mode);
#else
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        return -1;
    return fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
#endif
}
