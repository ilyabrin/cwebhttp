// Absolute minimal IOCP + AcceptEx test
// This will tell us if the Windows API itself works

#include <winsock2.h>
#include <windows.h>
#include <mswsock.h>
#include <stdio.h>

#pragma comment(lib, "ws2_32.lib")

int main(void)
{
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    printf("Creating IOCP...\n");
    HANDLE iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    printf("IOCP handle: %p\n\n", iocp);

    // Create listen socket with OVERLAPPED flag
    printf("Creating listen socket with WSA_FLAG_OVERLAPPED...\n");
    SOCKET listen_sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
    printf("Listen socket: %d\n\n", (int)listen_sock);

    // Bind and listen
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(9999);

    printf("Binding to port 9999...\n");
    bind(listen_sock, (struct sockaddr *)&addr, sizeof(addr));

    // Set to non-blocking mode
    printf("Setting listen socket to non-blocking...\n");
    u_long mode = 1;
    ioctlsocket(listen_sock, FIONBIO, &mode);

    listen(listen_sock, 5);
    printf("Listening\n\n");

    // Load AcceptEx
    printf("Loading AcceptEx...\n");
    LPFN_ACCEPTEX AcceptEx = NULL;
    GUID guid = WSAID_ACCEPTEX;
    DWORD bytes;
    WSAIoctl(listen_sock, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid),
             &AcceptEx, sizeof(AcceptEx), &bytes, NULL, NULL);
    printf("AcceptEx loaded: %p\n\n", AcceptEx);

    // Create accept socket with OVERLAPPED flag
    printf("Creating accept socket with WSA_FLAG_OVERLAPPED...\n");
    SOCKET accept_sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
    printf("Accept socket: %d\n\n", (int)accept_sock);

    // Associate the LISTEN socket with IOCP (try this approach)
    printf("Associating LISTEN socket with IOCP...\n");
    HANDLE h = CreateIoCompletionPort((HANDLE)listen_sock, iocp, (ULONG_PTR)1234, 0);
    printf("Result: %p\n", h);

    // ALSO associate accept socket
    printf("Associating accept socket with IOCP...\n");
    HANDLE h2 = CreateIoCompletionPort((HANDLE)accept_sock, iocp, (ULONG_PTR)1234, 0);
    printf("Result: %p\n\n", h2);

    // Prepare AcceptEx
    char buffer[128];
    OVERLAPPED ov = {0};
    DWORD received = 0;

    printf("Calling AcceptEx...\n");
    printf("  listen_sock=%d\n", (int)listen_sock);
    printf("  accept_sock=%d\n", (int)accept_sock);
    printf("  buffer=%p\n", buffer);
    printf("  dwReceiveDataLength=0\n");
    printf("  dwLocalAddressLength=32\n");
    printf("  dwRemoteAddressLength=32\n");
    printf("\n");

    BOOL ret = AcceptEx(listen_sock, accept_sock, buffer, 0, 32, 32, &received, &ov);
    DWORD err = WSAGetLastError();

    printf("AcceptEx returned: %d\n", ret);
    printf("WSAGetLastError(): %lu (%s)\n", err, err == WSA_IO_PENDING ? "WSA_IO_PENDING - OK!" : "ERROR");
    printf("bytes received: %lu\n\n", received);

    if (ret || err == WSA_IO_PENDING)
    {
        printf("=========================================\n");
        printf("SUCCESS: AcceptEx posted!\n");
        printf("=========================================\n\n");

        printf("Now connect with: telnet localhost 9999\n");
        printf("or: Test-NetConnection localhost -Port 9999\n\n");

        printf("Waiting for completion (30 second timeout)...\n");
        DWORD bytes_transferred = 0;
        ULONG_PTR completion_key = 0;
        LPOVERLAPPED overlapped_out = NULL;

        BOOL result = GetQueuedCompletionStatus(iocp, &bytes_transferred, &completion_key, &overlapped_out, 30000);
        DWORD wait_err = GetLastError();

        printf("DEBUG: result=%d, overlapped_out=%p\n", result, overlapped_out);

        printf("\n=========================================\n");
        printf("GetQueuedCompletionStatus returned: %d\n", result);
        printf("Error: %lu\n", wait_err);
        printf("Bytes transferred: %lu\n", bytes_transferred);
        printf("Completion key: %llu\n", (unsigned long long)completion_key);
        printf("Overlapped: %p (expected %p)\n", overlapped_out, &ov);
        printf("=========================================\n");

        if (result && overlapped_out == &ov)
        {
            printf("\n*** SUCCESS! AcceptEx completion received! ***\n");
            printf("*** IOCP + AcceptEx is WORKING! ***\n");
        }
        else if (wait_err == WAIT_TIMEOUT)
        {
            printf("\n*** TIMEOUT - No completion received ***\n");
            printf("*** This means AcceptEx completions are NOT being delivered! ***\n");
        }
        else
        {
            printf("\n*** ERROR - Something went wrong ***\n");
        }
    }
    else
    {
        printf("*** AcceptEx FAILED! ***\n");
    }

    closesocket(accept_sock);
    closesocket(listen_sock);
    CloseHandle(iocp);
    WSACleanup();

    return 0;
}
