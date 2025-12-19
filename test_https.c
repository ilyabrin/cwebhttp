#include "cwebhttp.h"
#include "cwebhttp_tls.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    printf("=== HTTPS Integration Test ===\n\n");

    // Check if TLS is available
    if (!cwh_tls_is_available())
    {
        printf("ERROR: TLS support not compiled in. Rebuild with CWEBHTTP_ENABLE_TLS=1\n");
        return 1;
    }

    printf("TLS support: Available\n\n");

    // Test 1: Simple HTTPS GET request
    printf("Test 1: HTTPS GET to https://www.example.com\n");
    cwh_response_t res1 = {0};
    cwh_error_t err = cwh_get("https://www.example.com", &res1);

    if (err == CWH_OK)
    {
        printf("  ✓ Status: %d\n", res1.status);
        if (res1.body && res1.body_len > 0)
        {
            printf("  ✓ Body length: %llu bytes\n", (unsigned long long)res1.body_len);
            // Check for expected content
            if (strstr(res1.body, "Example Domain"))
            {
                printf("  ✓ Content validated\n");
            }
            else
            {
                printf("  ✗ Expected content not found\n");
            }
        }
    }
    else
    {
        printf("  ✗ Request failed with error: %d\n", err);
        return 1;
    }

    printf("\n");

    // Test 2: HTTPS with different host
    printf("Test 2: HTTPS GET to https://httpbin.org/get\n");
    cwh_response_t res2 = {0};
    err = cwh_get("https://httpbin.org/get", &res2);

    if (err == CWH_OK)
    {
        printf("  ✓ Status: %d\n", res2.status);
        if (res2.body && res2.body_len > 0)
        {
            printf("  ✓ Body length: %llu bytes\n", (unsigned long long)res2.body_len);
        }
    }
    else
    {
        printf("  ✗ Request failed with error: %d\n", err);
        printf("  (httpbin.org might be unavailable, continuing...)\n");
    }

    printf("\n");

    // Test 3: HTTPS POST request
    printf("Test 3: HTTPS POST to https://httpbin.org/post\n");
    const char *post_data = "{\"test\":\"https_integration\"}";
    cwh_response_t res3 = {0};
    err = cwh_post("https://httpbin.org/post", post_data, strlen(post_data), &res3);

    if (err == CWH_OK)
    {
        printf("  ✓ Status: %d\n", res3.status);
        if (res3.body && res3.body_len > 0)
        {
            printf("  ✓ Body length: %llu bytes\n", (unsigned long long)res3.body_len);
        }
    }
    else
    {
        printf("  ✗ Request failed with error: %d\n", err);
        printf("  (httpbin.org might be unavailable, continuing...)\n");
    }

    printf("\n");

    // Test 4: Mixed HTTP and HTTPS
    printf("Test 4: Mixed HTTP and HTTPS requests\n");
    cwh_response_t res4 = {0};
    err = cwh_get("http://example.com", &res4);

    if (err == CWH_OK)
    {
        printf("  ✓ HTTP request successful (status: %d)\n", res4.status);
    }
    else
    {
        printf("  ✗ HTTP request failed: %d\n", err);
    }

    cwh_response_t res5 = {0};
    err = cwh_get("https://www.example.com", &res5);

    if (err == CWH_OK)
    {
        printf("  ✓ HTTPS request successful (status: %d)\n", res5.status);
    }
    else
    {
        printf("  ✗ HTTPS request failed: %d\n", err);
    }

    printf("\n");
    printf("=== HTTPS Integration Test Complete ===\n");

    // Cleanup
    cwh_pool_cleanup();

    return 0;
}
