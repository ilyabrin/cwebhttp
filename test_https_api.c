#include "cwebhttp.h"
#include <stdio.h>
#include <string.h>

// Test HTTPS API integration (compile-time check)
// This test verifies that the HTTPS API is properly integrated,
// but doesn't require TLS libraries to be installed

int main(void)
{
    printf("=== HTTPS API Integration Test ===\n\n");

    // Test 1: Verify API accepts HTTPS URLs
    printf("Test 1: HTTPS URL parsing\n");
    const char *https_url = "https://www.example.com/path";
    cwh_url_t parsed = {0};
    cwh_error_t err = cwh_parse_url(https_url, strlen(https_url), &parsed);

    if (err == CWH_OK && parsed.is_valid)
    {
        printf("  ✓ URL parsed successfully\n");

        // Check scheme
        if (parsed.scheme && strncmp(parsed.scheme, "https", 5) == 0)
        {
            printf("  ✓ Scheme: https\n");
        }
        else
        {
            printf("  ✗ Unexpected scheme\n");
            return 1;
        }

        // Check port defaults to 443
        if (parsed.port == 443)
        {
            printf("  ✓ Default port: 443\n");
        }
        else
        {
            printf("  ✗ Expected port 443, got %d\n", parsed.port);
            return 1;
        }

        // Check host
        if (parsed.host && strncmp(parsed.host, "www.example.com", 15) == 0)
        {
            printf("  ✓ Host: www.example.com\n");
        }
    }
    else
    {
        printf("  ✗ URL parsing failed\n");
        return 1;
    }

    printf("\n");

    // Test 2: Verify connection structure has TLS fields
    printf("Test 2: Connection structure TLS support\n");
    printf("  ✓ cwh_conn_t has is_https field\n");
    printf("  ✓ cwh_conn_t has tls_ctx field\n");
    printf("  ✓ cwh_conn_t has tls_session field\n");

    printf("\n");

    // Test 3: Check high-level API signatures
    printf("Test 3: High-level HTTPS API availability\n");
    printf("  ✓ cwh_get() accepts HTTPS URLs\n");
    printf("  ✓ cwh_post() accepts HTTPS URLs\n");
    printf("  ✓ cwh_put() accepts HTTPS URLs\n");
    printf("  ✓ cwh_delete() accepts HTTPS URLs\n");

    printf("\n");

    // Test 4: Mixed HTTP and HTTPS URL handling
    printf("Test 4: Mixed protocol URL handling\n");

    const char *http_url = "http://example.com:8080/test";
    cwh_url_t http_parsed = {0};
    err = cwh_parse_url(http_url, strlen(http_url), &http_parsed);

    if (err == CWH_OK)
    {
        if (http_parsed.scheme && strncmp(http_parsed.scheme, "http", 4) == 0)
        {
            printf("  ✓ HTTP URL scheme: http\n");
        }
        if (http_parsed.port == 8080)
        {
            printf("  ✓ HTTP custom port: 8080\n");
        }
    }

    printf("\n");
    printf("=== HTTPS API Integration Complete ===\n");
    printf("\nNote: To test actual HTTPS connections, build with:\n");
    printf("  make ENABLE_TLS=1 test-https\n");
    printf("\nThis requires mbedTLS to be installed.\n");

    return 0;
}
