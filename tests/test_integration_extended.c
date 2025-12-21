// Integration tests with real HTTP servers
// Tests actual network functionality, not just parsing

#include "cwebhttp.h"
#include "unity.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void setUp(void) {}
void tearDown(void) {}

// Test GET request to example.com
void test_real_http_get(void)
{
    cwh_response_t res = {0};
    cwh_error_t err = cwh_get("http://example.com/", &res);

    TEST_ASSERT_EQUAL(CWH_OK, err);
    TEST_ASSERT_TRUE(res.is_valid);
    TEST_ASSERT_EQUAL(200, res.status);
    TEST_ASSERT_NOT_NULL(res.body);
    TEST_ASSERT_GREATER_THAN(0, res.body_len);
}

// Test HTTPS request (if TLS enabled)
void test_real_https_get(void)
{
#if CWEBHTTP_ENABLE_TLS
    cwh_response_t res = {0};
    cwh_error_t err = cwh_get("https://example.com/", &res);

    TEST_ASSERT_EQUAL(CWH_OK, err);
    TEST_ASSERT_TRUE(res.is_valid);
    TEST_ASSERT_EQUAL(200, res.status);
#else
    TEST_IGNORE_MESSAGE("TLS not enabled");
#endif
}

// Test 404 error handling
void test_real_http_404(void)
{
    cwh_response_t res = {0};
    cwh_error_t err = cwh_get("http://example.com/nonexistent-page-12345", &res);

    TEST_ASSERT_EQUAL(CWH_OK, err); // Request succeeded
    TEST_ASSERT_TRUE(res.is_valid);
    TEST_ASSERT_EQUAL(404, res.status); // But got 404
}

// Test redirect following
void test_real_http_redirect(void)
{
#if CWEBHTTP_ENABLE_REDIRECTS
    cwh_response_t res = {0};
    // httpbin.org/redirect/1 redirects to /get
    cwh_error_t err = cwh_get("http://httpbin.org/redirect/1", &res);

    TEST_ASSERT_EQUAL(CWH_OK, err);
    TEST_ASSERT_TRUE(res.is_valid);
    TEST_ASSERT_EQUAL(200, res.status);
#else
    TEST_IGNORE_MESSAGE("Redirects not enabled");
#endif
}

// Test gzip compression
void test_real_http_gzip(void)
{
#if CWEBHTTP_ENABLE_COMPRESSION
    cwh_response_t res = {0};
    cwh_error_t err = cwh_get("http://httpbin.org/gzip", &res);

    TEST_ASSERT_EQUAL(CWH_OK, err);
    TEST_ASSERT_TRUE(res.is_valid);
    TEST_ASSERT_EQUAL(200, res.status);
    // Response should be automatically decompressed
    TEST_ASSERT_NOT_NULL(res.body);
    TEST_ASSERT_GREATER_THAN(0, res.body_len);
#else
    TEST_IGNORE_MESSAGE("Compression not enabled");
#endif
}

// Test connection timeout
void test_connection_timeout(void)
{
    // Try to connect to a non-routable IP (should timeout)
    cwh_response_t res = {0};
    cwh_error_t err = cwh_get("http://192.0.2.1:81/", &res);

    // Should fail with network or timeout error
    TEST_ASSERT_NOT_EQUAL(CWH_OK, err);
}

// Test invalid URL
void test_invalid_url(void)
{
    cwh_response_t res = {0};
    cwh_error_t err = cwh_get("not-a-valid-url", &res);

    TEST_ASSERT_NOT_EQUAL(CWH_OK, err);
}

// Test large response
void test_large_response(void)
{
    cwh_response_t res = {0};
    // httpbin.org/bytes/N returns N random bytes
    cwh_error_t err = cwh_get("http://httpbin.org/bytes/1024", &res);

    TEST_ASSERT_EQUAL(CWH_OK, err);
    TEST_ASSERT_TRUE(res.is_valid);
    TEST_ASSERT_EQUAL(200, res.status);
    TEST_ASSERT_EQUAL(1024, res.body_len);
}

// Test POST request
void test_real_http_post(void)
{
    cwh_response_t res = {0};
    const char *json_data = "{\"test\":\"data\"}";

    // Simple POST - just verify it doesn't crash
    // Note: This is a simplified test
    cwh_conn_t *conn = cwh_connect("http://httpbin.org/post", 30000);
    if (conn)
    {
        const char *headers[] = {
            "Content-Type", "application/json",
            NULL};
        cwh_send_req(conn, CWH_METHOD_POST, "/post", headers, json_data, strlen(json_data));
        cwh_error_t err = cwh_read_res(conn, &res);
        cwh_close(conn);

        TEST_ASSERT_EQUAL(CWH_OK, err);
        TEST_ASSERT_TRUE(res.is_valid);
        TEST_ASSERT_EQUAL(200, res.status);
    }
    else
    {
        TEST_FAIL_MESSAGE("Connection failed");
    }
}

int main(void)
{
    UNITY_BEGIN();

    printf("\n=== cwebhttp Integration Tests (Real Network) ===\n");
    printf("Note: These tests require internet connection\n\n");

    RUN_TEST(test_real_http_get);
    RUN_TEST(test_real_http_404);
    RUN_TEST(test_real_http_redirect);
    RUN_TEST(test_real_http_gzip);
    RUN_TEST(test_real_https_get);
    RUN_TEST(test_large_response);
    RUN_TEST(test_real_http_post);
    RUN_TEST(test_connection_timeout);
    RUN_TEST(test_invalid_url);

    return UNITY_END();
}
