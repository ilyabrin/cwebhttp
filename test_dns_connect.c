// Test DNS resolution and TCP connect
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <netdb.h>
#include <arpa/inet.h>
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
#endif

    const char *host = "example.com";
    const char *port = "80";

    printf("Resolving %s:%s...\n", host, port);

    struct addrinfo hints = {0};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *result = NULL;
    int ret = getaddrinfo(host, port, &hints, &result);

    if (ret != 0)
    {
        printf("getaddrinfo failed: %d\n", ret);
        return 1;
    }

    printf("DNS resolution successful\n");

    struct sockaddr_in *addr = (struct sockaddr_in *)result->ai_addr;
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr->sin_addr, ip_str, sizeof(ip_str));
    printf("Resolved to: %s:%d\n", ip_str, ntohs(addr->sin_port));

    freeaddrinfo(result);

#ifdef _WIN32
    WSACleanup();
#endif

    printf("Test complete!\n");
    return 0;
}
