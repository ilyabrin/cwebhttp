#include "unity.h"
#include "cwebhttp.h"
#include <string.h>

void setUp(void)
{
    // Set up test fixtures (if needed)
}

void tearDown(void)
{
    // Clean up test fixtures (if needed)
}

// Helper to compare string with length
static int str_eq_len(const char *a, const char *b, size_t len)
{
    return memcmp(a, b, len) == 0;
}

void test_parse_get_simple()
{
    char buf[] = "GET / HTTP/1.1\r\nHost: test\r\n\r\n";
    cwh_request_t req = {0};
    TEST_ASSERT_EQUAL(CWH_OK, cwh_parse_req(buf, strlen(buf), &req));
    TEST_ASSERT_TRUE(req.is_valid);
    TEST_ASSERT_TRUE(str_eq_len(req.method_str, "GET", 3));
    TEST_ASSERT_TRUE(str_eq_len(req.path, "/", 1));
}

void test_parse_post_with_body()
{
    char buf[] = "POST /api/users HTTP/1.1\r\n"
                 "Content-Type: application/json\r\n"
                 "Content-Length: 13\r\n"
                 "\r\n"
                 "{\"key\":\"val\"}";
    cwh_request_t req = {0};
    TEST_ASSERT_EQUAL(CWH_OK, cwh_parse_req(buf, strlen(buf), &req));
    TEST_ASSERT_TRUE(req.is_valid);
    TEST_ASSERT_TRUE(str_eq_len(req.method_str, "POST", 4));
    TEST_ASSERT_TRUE(str_eq_len(req.path, "/api/users", 10));
    TEST_ASSERT_EQUAL(2, req.num_headers);
    TEST_ASSERT_EQUAL(13, req.body_len);
    TEST_ASSERT_TRUE(str_eq_len(req.body, "{\"key\":\"val\"}", 13));
}

void test_parse_get_with_query()
{
    char buf[] = "GET /search?q=hello&lang=en HTTP/1.1\r\n"
                 "Host: example.com\r\n"
                 "\r\n";
    cwh_request_t req = {0};
    TEST_ASSERT_EQUAL(CWH_OK, cwh_parse_req(buf, strlen(buf), &req));
    TEST_ASSERT_TRUE(req.is_valid);
    TEST_ASSERT_TRUE(str_eq_len(req.path, "/search", 7));
    TEST_ASSERT_NOT_NULL(req.query);
    TEST_ASSERT_TRUE(str_eq_len(req.query, "q=hello&lang=en", 15));
}

void test_parse_put()
{
    char buf[] = "PUT /resource/123 HTTP/1.1\r\n"
                 "Authorization: Bearer token123\r\n"
                 "\r\n";
    cwh_request_t req = {0};
    TEST_ASSERT_EQUAL(CWH_OK, cwh_parse_req(buf, strlen(buf), &req));
    TEST_ASSERT_TRUE(req.is_valid);
    TEST_ASSERT_TRUE(str_eq_len(req.method_str, "PUT", 3));
    TEST_ASSERT_TRUE(str_eq_len(req.path, "/resource/123", 13));
    TEST_ASSERT_EQUAL(1, req.num_headers);
}

void test_parse_delete()
{
    char buf[] = "DELETE /users/42 HTTP/1.1\r\n\r\n";
    cwh_request_t req = {0};
    TEST_ASSERT_EQUAL(CWH_OK, cwh_parse_req(buf, strlen(buf), &req));
    TEST_ASSERT_TRUE(req.is_valid);
    TEST_ASSERT_TRUE(str_eq_len(req.method_str, "DELETE", 6));
    TEST_ASSERT_TRUE(str_eq_len(req.path, "/users/42", 9));
}

void test_parse_multiple_headers()
{
    char buf[] = "GET /test HTTP/1.1\r\n"
                 "Host: example.com\r\n"
                 "User-Agent: cwebhttp/0.1\r\n"
                 "Accept: */*\r\n"
                 "Connection: keep-alive\r\n"
                 "\r\n";
    cwh_request_t req = {0};
    TEST_ASSERT_EQUAL(CWH_OK, cwh_parse_req(buf, strlen(buf), &req));
    TEST_ASSERT_TRUE(req.is_valid);
    TEST_ASSERT_EQUAL(4, req.num_headers);

    // Check first header
    TEST_ASSERT_TRUE(str_eq_len(req.headers[0], "Host", 4));
    TEST_ASSERT_TRUE(str_eq_len(req.headers[1], "example.com", 11));
}

void test_parse_invalid_no_method()
{
    char buf[] = " /path HTTP/1.1\r\n\r\n";
    cwh_request_t req = {0};
    TEST_ASSERT_EQUAL(CWH_ERR_PARSE, cwh_parse_req(buf, strlen(buf), &req));
}

void test_parse_invalid_bad_version()
{
    char buf[] = "GET /path HTTP/2.0\r\n\r\n";
    cwh_request_t req = {0};
    TEST_ASSERT_EQUAL(CWH_ERR_PARSE, cwh_parse_req(buf, strlen(buf), &req));
}

void test_parse_null_buffer()
{
    cwh_request_t req = {0};
    TEST_ASSERT_EQUAL(CWH_ERR_PARSE, cwh_parse_req(NULL, 100, &req));
}

void test_parse_empty_buffer()
{
    char buf[] = "";
    cwh_request_t req = {0};
    TEST_ASSERT_EQUAL(CWH_ERR_PARSE, cwh_parse_req(buf, 0, &req));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_parse_get_simple);
    RUN_TEST(test_parse_post_with_body);
    RUN_TEST(test_parse_get_with_query);
    RUN_TEST(test_parse_put);
    RUN_TEST(test_parse_delete);
    RUN_TEST(test_parse_multiple_headers);
    RUN_TEST(test_parse_invalid_no_method);
    RUN_TEST(test_parse_invalid_bad_version);
    RUN_TEST(test_parse_null_buffer);
    RUN_TEST(test_parse_empty_buffer);
    return UNITY_END();
}