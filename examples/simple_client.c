#include "cwebhttp.h"
#include <stdio.h>
#include <string.h>

int main()
{
    printf("=== cwebhttp Simple Client Demo ===\n\n");

    // Example 1: High-level GET (one-liner)
    printf("1. Testing high-level GET API...\n");
    cwh_response_t res1 = {0};
    cwh_error_t err = cwh_get("http://httpbin.org/get", &res1);
    if (err == CWH_OK)
    {
        printf("   Status: %d\n", res1.status);
        printf("   Body: %.*s\n\n", (int)res1.body_len, res1.body);
    }
    else
    {
        printf("   GET failed: %d\n\n", err);
    }

    // Example 2: High-level POST
    printf("2. Testing high-level POST API...\n");
    const char *post_data = "{\"name\":\"cwebhttp\",\"version\":\"0.1.0\"}";
    cwh_response_t res2 = {0};
    err = cwh_post("http://httpbin.org/post", post_data, strlen(post_data), &res2);
    if (err == CWH_OK)
    {
        printf("   Status: %d\n", res2.status);
        printf("   Body: %.*s\n\n", (int)res2.body_len, res2.body);
    }
    else
    {
        printf("   POST failed: %d\n\n", err);
    }

    // Example 3: Low-level API (manual connection)
    printf("3. Testing low-level API...\n");
    cwh_conn_t *conn = cwh_connect("http://httpbin.org/headers", 5000);
    if (!conn)
    {
        printf("   Connect fail\n");
        return 1;
    }

    err = cwh_send_req(conn, CWH_METHOD_GET, "/headers", NULL, NULL, 0);
    if (err != CWH_OK)
    {
        printf("   Send fail: %d\n", err);
    }
    else
    {
        cwh_response_t res3 = {0};
        err = cwh_read_res(conn, &res3);
        if (err == CWH_OK)
        {
            printf("   Status: %d\n", res3.status);
            printf("   Body: %.*s\n", (int)res3.body_len, res3.body);
        }
    }
    cwh_close(conn);

    printf("\n=== Demo Complete ===\n");
    return 0;
}