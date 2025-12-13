#include "cwebhttp.h"
#include <stdio.h>

int main()
{
    printf("Testing HTTP redirect following...\n\n");

    // Test 1: Single redirect (302)
    printf("Test 1: Single redirect (302)\n");
    printf("Requesting http://httpbin.org/redirect/1\n");

    cwh_response_t res = {0};
    cwh_error_t err = cwh_get("http://httpbin.org/redirect/1", &res);

    if (err == CWH_OK)
    {
        printf("✓ SUCCESS: Final status = %d\n", res.status);
        if (res.status == 200)
            printf("✓ Redirect followed successfully!\n");
        else
            printf("✗ FAIL: Expected status 200, got %d\n", res.status);
    }
    else
    {
        printf("✗ FAIL: Request error = %d\n", err);
    }

    printf("\n");

    // Test 2: Multiple redirects
    printf("Test 2: Multiple redirects (3 hops)\n");
    printf("Requesting http://httpbin.org/redirect/3\n");

    cwh_response_t res2 = {0};
    err = cwh_get("http://httpbin.org/redirect/3", &res2);

    if (err == CWH_OK)
    {
        printf("✓ SUCCESS: Final status = %d\n", res2.status);
        if (res2.status == 200)
            printf("✓ Multiple redirects followed successfully!\n");
        else
            printf("✗ FAIL: Expected status 200, got %d\n", res2.status);
    }
    else
    {
        printf("✗ FAIL: Request error = %d\n", err);
    }

    printf("\n");

    // Test 3: Too many redirects (should fail)
    printf("Test 3: Too many redirects (20 hops - should fail)\n");
    printf("Requesting http://httpbin.org/redirect/20\n");

    cwh_response_t res3 = {0};
    err = cwh_get("http://httpbin.org/redirect/20", &res3);

    if (err != CWH_OK)
    {
        printf("✓ SUCCESS: Correctly rejected too many redirects (error = %d)\n", err);
    }
    else
    {
        printf("✗ FAIL: Should have rejected, but got status = %d\n", res3.status);
    }

    printf("\n");
    printf("Redirect testing complete!\n");

    return 0;
}
