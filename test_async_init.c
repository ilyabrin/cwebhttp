// Test async client initialization
#include "include/cwebhttp_async.h"
#include <stdio.h>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <signal.h>
#endif

void test_callback(cwh_response_t *res, cwh_error_t err, void *data)
{
    printf("Callback called! err=%d\n", err);
}

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

    printf("Initiating async GET request...\n");
    cwh_async_get(loop, "http://example.com/", test_callback, NULL);

    printf("Request initiated. NOT running loop (just testing initialization)\n");

    // Don't run loop, just cleanup
    printf("Cleaning up...\n");
    cwh_loop_free(loop);

#ifdef _WIN32
    WSACleanup();
#endif

    printf("Done!\n");
    return 0;
}
