// Simple test to check if async client initializes properly
#include "include/cwebhttp_async.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <signal.h>
#endif

int main(void)
{
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        fprintf(stderr, "Failed to initialize Winsock\n");
        return 1;
    }
#else
    signal(SIGPIPE, SIG_IGN);
#endif

    printf("Creating event loop...\n");
    cwh_loop_t *loop = cwh_loop_new();
    if (!loop)
    {
        fprintf(stderr, "Failed to create event loop\n");
        return 1;
    }

    printf("Event loop backend: %s\n", cwh_loop_backend(loop));
    printf("Event loop created successfully\n");

    // Don't run the loop, just test creation
    printf("Cleaning up...\n");
    cwh_loop_free(loop);

#ifdef _WIN32
    WSACleanup();
#endif

    printf("Done!\n");
    return 0;
}
