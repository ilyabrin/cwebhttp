#include "cwebhttp.h"
#include <stdio.h>

int main()
{
    cwh_conn_t *conn = cwh_connect("http://example.com", 5000);
    if (!conn)
    {
        printf("Connect fail\n");
        return 1;
    }
    cwh_error_t err = cwh_send_req(conn, CWH_METHOD_GET, "/", NULL, NULL, 0);
    if (err != CWH_OK)
    {
        printf("Send fail: %d\n", err);
    }
    else
    {
        cwh_response_t res = {0};
        err = cwh_read_res(conn, &res);
        if (err == CWH_OK)
        {
            printf("Status: %d, Body: %.*s\n", res.status, (int)res.body_len, res.body);
        }
    }
    cwh_close(conn);
    return 0;
}