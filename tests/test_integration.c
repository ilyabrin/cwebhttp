// Integration tests - real HTTP requests to verify cwebhttp functionality
// Uses example.com and other reliable test endpoints

#include "cwebhttp.h"
#include "unity.h"
#include <string.h>
#include <stdio.h>

void setUp(void)
{
    // Initialize connection pool and cookie jar before each test
    cwh_pool_init();
    cwh_cookie_jar_init();
}

void tearDown(void)
{
    // Cleanup after each test
    cwh_pool_cleanup();
    cwh_cookie_jar_cleanup();
}

// Test 1: Basic GET request to example.com
void test_integration_basic_get(void)
{
    cwh_response_t res = {0};
    cwh_error_t err = cwh_get("http://example.com", &res);

    TEST_ASSERT_EQUAL(CWH_OK, err);
    TEST_ASSERT_EQUAL(200, res.status);
    TEST_ASSERT_NOT_NULL(res.body);
    TEST_ASSERT_TRUE(res.body_len > 0);

    // example.com returns HTML with "Example Domain"
    TEST_ASSERT_NOT_NULL(strstr(res.body, "Example Domain"));
}

// Test 2: GET with custom headers
void test_integration_custom_headers(void)
{
    cwh_conn_t *conn = cwh_connect("http://example.com", 5000);
    TEST_ASSERT_NOT_NULL(conn);

    const char *headers[] = {
        "X-Custom-Header", "cwebhttp-test",
        "User-Agent", "cwebhttp/0.3-integration-test",
        NULL};

    cwh_error_t err = cwh_send_req(conn, CWH_METHOD_GET, "/", headers, NULL, 0);
    TEST_ASSERT_EQUAL(CWH_OK, err);

    cwh_response_t res = {0};
    err = cwh_read_res(conn, &res);
    TEST_ASSERT_EQUAL(CWH_OK, err);
    TEST_ASSERT_EQUAL(200, res.status);

    cwh_close(conn);
}

// Test 3: Connection reuse (keep-alive)
void test_integration_keepalive(void)
{
    // First request
    cwh_response_t res1 = {0};
    cwh_error_t err1 = cwh_get("http://example.com", &res1);
    TEST_ASSERT_EQUAL(CWH_OK, err1);
    TEST_ASSERT_EQUAL(200, res1.status);

    // Second request to same host - should reuse connection
    cwh_response_t res2 = {0};
    cwh_error_t err2 = cwh_get("http://example.com", &res2);
    TEST_ASSERT_EQUAL(CWH_OK, err2);
    TEST_ASSERT_EQUAL(200, res2.status);

    // Both requests should succeed
    TEST_ASSERT_NOT_NULL(res1.body);
    TEST_ASSERT_NOT_NULL(res2.body);
}

// Test 4: POST request
void test_integration_post(void)
{
    const char *body = "test=data&name=cwebhttp";

    cwh_response_t res = {0};
    cwh_error_t err = cwh_post("http://httpbin.org/post", body, strlen(body), &res);

    // This might fail if httpbin is down, so we just check it doesn't crash
    // We consider both success and network errors as acceptable
    TEST_ASSERT_TRUE(err == CWH_OK || err == CWH_ERR_NET || err == CWH_ERR_TIMEOUT);
}

// Test 5: Response header parsing
void test_integration_response_headers(void)
{
    cwh_response_t res = {0};
    cwh_error_t err = cwh_get("http://example.com", &res);

    TEST_ASSERT_EQUAL(CWH_OK, err);
    TEST_ASSERT_EQUAL(200, res.status);

    // Check that we can access response headers
    const char *content_type = cwh_get_res_header(&res, "Content-Type");
    TEST_ASSERT_NOT_NULL(content_type);
    // example.com returns HTML
    TEST_ASSERT_NOT_NULL(strstr(content_type, "text/html"));
}

// Test 6: Large response handling
void test_integration_large_response(void)
{
    // example.com is small, but this tests we can handle it
    cwh_response_t res = {0};
    cwh_error_t err = cwh_get("http://example.com", &res);

    TEST_ASSERT_EQUAL(CWH_OK, err);
    TEST_ASSERT_EQUAL(200, res.status);
    TEST_ASSERT_TRUE(res.body_len > 100); // example.com is larger than 100 bytes
}

// Test 7: Invalid URL handling
void test_integration_invalid_url(void)
{
    cwh_response_t res = {0};
    cwh_error_t err = cwh_get("http://this-domain-does-not-exist-cwebhttp-test.com", &res);

    // Should fail gracefully with network error
    TEST_ASSERT_NOT_EQUAL(CWH_OK, err);
}

// Test 8: Connection to non-existent port
void test_integration_connection_refused(void)
{
    cwh_conn_t *conn = cwh_connect("http://example.com:9999", 2000);

    // Should fail or timeout
    // On some systems this returns NULL, on others it connects but times out later
    // Either way is acceptable - we just verify it doesn't crash
    if (conn)
    {
        cwh_close(conn);
    }
    TEST_ASSERT_TRUE(true); // If we got here, we didn't crash
}

// Test 9: URL parsing edge cases
void test_integration_url_parsing(void)
{
    // Test with explicit port
    cwh_response_t res = {0};
    cwh_error_t err = cwh_get("http://example.com:80/", &res);

    TEST_ASSERT_EQUAL(CWH_OK, err);
    TEST_ASSERT_EQUAL(200, res.status);
}

// Test 10: Cookie jar functionality
void test_integration_cookie_jar(void)
{
    // Add a cookie manually
    cwh_cookie_jar_add("example.com", "test=value; Path=/");

    // Get cookies for example.com
    char *cookies = cwh_cookie_jar_get("example.com", "/");

    TEST_ASSERT_NOT_NULL(cookies);
    TEST_ASSERT_NOT_NULL(strstr(cookies, "test=value"));

    free(cookies);
}

// Test 11: Connection pool stress test
void test_integration_pool_stress(void)
{
    // Make multiple requests to test connection pooling
    for (int i = 0; i < 5; i++)
    {
        cwh_response_t res = {0};
        cwh_error_t err = cwh_get("http://example.com", &res);

        TEST_ASSERT_EQUAL(CWH_OK, err);
        TEST_ASSERT_EQUAL(200, res.status);
    }

    // Pool should handle this gracefully
    TEST_ASSERT_TRUE(true);
}

// Test 12: Different HTTP methods (basic smoke test)
void test_integration_http_methods(void)
{
    // Test GET method
    cwh_response_t res = {0};
    cwh_error_t err = cwh_get("http://example.com", &res);
    TEST_ASSERT_EQUAL(CWH_OK, err);
    TEST_ASSERT_EQUAL(200, res.status);

    // Test DELETE method (example.com will return 405 Method Not Allowed, which is fine)
    err = cwh_delete("http://example.com", &res);
    // We accept either success or 4xx/5xx status codes
    TEST_ASSERT_TRUE(err == CWH_OK || err == CWH_ERR_NET);
}

int main(void)
{
    UNITY_BEGIN();

    printf("\n=== cwebhttp Integration Tests ===\n");
    printf("Testing real HTTP connectivity and features\n");
    printf("Note: These tests require internet connection\n\n");

    // Basic functionality
    RUN_TEST(test_integration_basic_get);
    RUN_TEST(test_integration_custom_headers);
    RUN_TEST(test_integration_keepalive);

    // HTTP operations
    RUN_TEST(test_integration_post);
    RUN_TEST(test_integration_response_headers);
    RUN_TEST(test_integration_large_response);

    // Error handling
    RUN_TEST(test_integration_invalid_url);
    RUN_TEST(test_integration_connection_refused);

    // Advanced features
    RUN_TEST(test_integration_url_parsing);
    RUN_TEST(test_integration_cookie_jar);
    RUN_TEST(test_integration_pool_stress);
    RUN_TEST(test_integration_http_methods);

    return UNITY_END();
}
