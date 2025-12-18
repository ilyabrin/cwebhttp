// Test DNS resolution and connection
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#endif
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#endif

int main(void)
{
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    printf("Testing connection to localhost:8080...\n");

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        printf("Failed to create socket\n");
        return 1;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    printf("Attempting to connect...\n");
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        printf("Connection failed\n");
#ifdef _WIN32
        printf("Error: %d\n", WSAGetLastError());
        closesocket(sock);
#else
        perror("connect");
        close(sock);
#endif
        return 1;
    }

    printf("Connected successfully!\n");

    const char *request = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
    printf("Sending request...\n");
    send(sock, request, strlen(request), 0);

    char buffer[1024] = {0};
    printf("Waiting for response...\n");
    int n = recv(sock, buffer, sizeof(buffer) - 1, 0);

    if (n > 0)
    {
        buffer[n] = '\0';
        printf("Received %d bytes:\n%s\n", n, buffer);
    }
    else
    {
        printf("No response received\n");
    }

#ifdef _WIN32
    closesocket(sock);
    WSACleanup();
#else
    close(sock);
#endif

    return 0;
}
