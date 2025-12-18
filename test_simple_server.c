// Simple test server to verify basic HTTP functionality
#include "cwebhttp.h"
#include <stdio.h>
#include <string.h>

cwh_error_t handle_root(cwh_request_t *req, cwh_conn_t *conn, void *user_data)
{
    (void)req;
    (void)user_data;

    printf("[SERVER] Handling request for: %s\n", req->path);

    const char *response = "Hello, World!";
    return cwh_send_response(conn, 200, "text/plain", response, strlen(response));
}

int main(void)
{
    printf("[SERVER] Starting simple test server on port 8080...\n");

    cwh_server_t *server = cwh_server_new();
    if (!server)
    {
        fprintf(stderr, "[ERROR] Failed to create server\n");
        return 1;
    }

    cwh_server_route(server, "GET", "/", handle_root, NULL);

    printf("[SERVER] Server created, listening on port 8080...\n");

    if (cwh_server_listen(server, 8080) != CWH_OK)
    {
        fprintf(stderr, "[ERROR] Failed to listen on port 8080\n");
        cwh_server_free(server);
        return 1;
    }

    printf("[SERVER] Server listening, press Ctrl+C to stop\n");

    cwh_server_free(server);
    return 0;
}
