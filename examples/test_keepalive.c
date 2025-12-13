#include "cwebhttp.h"
#include <stdio.h>
#include <string.h>

int main()
{
    printf("=== Testing HTTP Connection Keep-Alive ===\n\n");

    // Test multiple requests to the same server
    const char *test_url = "http://httpbin.org/get";

    printf("Making 3 consecutive requests to: %s\n", test_url);
    printf("With keep-alive, the same connection should be reused.\n\n");

    for (int i = 1; i <= 3; i++)
    {
        printf("Request #%d:\n", i);

        cwh_response_t res = {0};
        cwh_error_t err = cwh_get(test_url, &res);

        if (err == CWH_OK)
        {
            printf("  ✓ Status: %d\n", res.status);

            // Check if Connection header indicates keep-alive
            const char *connection = cwh_get_res_header(&res, "Connection");
            if (connection)
            {
                printf("  ✓ Connection: %s\n", connection);
            }
        }
        else
        {
            printf("  ✗ Request failed with error: %d\n", err);
        }

        printf("\n");
    }

    // Cleanup connection pool
    cwh_pool_cleanup();

    printf("Test complete!\n");
    printf("\nNote: Check implementation - connections to same host:port should be reused.\n");

    return 0;
}
