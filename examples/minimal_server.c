#include "cwebhttp.h"
#include <stdio.h>

cwh_error_t dummy_handler(cwh_request_t *req, cwh_conn_t *conn, void *data)
{
    (void)req;
    (void)conn;
    (void)data;
    printf("Handler called!\n");
    return CWH_OK;
}

int main()
{
    cwh_server_t *srv = cwh_listen("0.0.0.0:8080", 128);
    if (!srv)
    {
        printf("Listen fail\n");
        return 1;
    }
    cwh_route(srv, "GET", "/", dummy_handler, NULL);
    printf("Starting dummy server...\n");
    cwh_run(srv);
    cwh_free_server(srv);
    return 0;
}