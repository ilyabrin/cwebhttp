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

void test_url_simple_http()
{
    char url[] = "http://example.com";
    cwh_url_t parsed = {0};
    TEST_ASSERT_EQUAL(CWH_OK, cwh_parse_url(url, strlen(url), &parsed));
    TEST_ASSERT_TRUE(parsed.is_valid);
    TEST_ASSERT_TRUE(str_eq_len(parsed.scheme, "http", 4));
    TEST_ASSERT_TRUE(str_eq_len(parsed.host, "example.com", 11));
    TEST_ASSERT_EQUAL(80, parsed.port);
    TEST_ASSERT_NULL(parsed.port_str);
    TEST_ASSERT_NULL(parsed.path);
    TEST_ASSERT_NULL(parsed.query);
    TEST_ASSERT_NULL(parsed.fragment);
}

void test_url_https_with_path()
{
    char url[] = "https://api.github.com/repos";
    cwh_url_t parsed = {0};
    TEST_ASSERT_EQUAL(CWH_OK, cwh_parse_url(url, strlen(url), &parsed));
    TEST_ASSERT_TRUE(parsed.is_valid);
    TEST_ASSERT_TRUE(str_eq_len(parsed.scheme, "https", 5));
    TEST_ASSERT_TRUE(str_eq_len(parsed.host, "api.github.com", 14));
    TEST_ASSERT_EQUAL(443, parsed.port);
    TEST_ASSERT_TRUE(str_eq_len(parsed.path, "/repos", 6));
    TEST_ASSERT_NULL(parsed.query);
}

void test_url_custom_port()
{
    char url[] = "http://localhost:8080/api";
    cwh_url_t parsed = {0};
    TEST_ASSERT_EQUAL(CWH_OK, cwh_parse_url(url, strlen(url), &parsed));
    TEST_ASSERT_TRUE(parsed.is_valid);
    TEST_ASSERT_TRUE(str_eq_len(parsed.host, "localhost", 9));
    TEST_ASSERT_EQUAL(8080, parsed.port);
    TEST_ASSERT_TRUE(str_eq_len(parsed.port_str, "8080", 4));
    TEST_ASSERT_TRUE(str_eq_len(parsed.path, "/api", 4));
}

void test_url_with_query()
{
    char url[] = "http://example.com/search?q=hello&lang=en";
    cwh_url_t parsed = {0};
    TEST_ASSERT_EQUAL(CWH_OK, cwh_parse_url(url, strlen(url), &parsed));
    TEST_ASSERT_TRUE(parsed.is_valid);
    TEST_ASSERT_TRUE(str_eq_len(parsed.host, "example.com", 11));
    TEST_ASSERT_TRUE(str_eq_len(parsed.path, "/search", 7));
    TEST_ASSERT_TRUE(str_eq_len(parsed.query, "q=hello&lang=en", 15));
    TEST_ASSERT_NULL(parsed.fragment);
}

void test_url_with_fragment()
{
    char url[] = "https://example.com/docs#section";
    cwh_url_t parsed = {0};
    TEST_ASSERT_EQUAL(CWH_OK, cwh_parse_url(url, strlen(url), &parsed));
    TEST_ASSERT_TRUE(parsed.is_valid);
    TEST_ASSERT_TRUE(str_eq_len(parsed.path, "/docs", 5));
    TEST_ASSERT_TRUE(str_eq_len(parsed.fragment, "section", 7));
    TEST_ASSERT_NULL(parsed.query);
}

void test_url_with_query_and_fragment()
{
    char url[] = "http://example.com/page?id=123#top";
    cwh_url_t parsed = {0};
    TEST_ASSERT_EQUAL(CWH_OK, cwh_parse_url(url, strlen(url), &parsed));
    TEST_ASSERT_TRUE(parsed.is_valid);
    TEST_ASSERT_TRUE(str_eq_len(parsed.path, "/page", 5));
    TEST_ASSERT_TRUE(str_eq_len(parsed.query, "id=123", 6));
    TEST_ASSERT_TRUE(str_eq_len(parsed.fragment, "top", 3));
}

void test_url_ip_address()
{
    char url[] = "http://192.168.1.1:3000/status";
    cwh_url_t parsed = {0};
    TEST_ASSERT_EQUAL(CWH_OK, cwh_parse_url(url, strlen(url), &parsed));
    TEST_ASSERT_TRUE(parsed.is_valid);
    TEST_ASSERT_TRUE(str_eq_len(parsed.host, "192.168.1.1", 11));
    TEST_ASSERT_EQUAL(3000, parsed.port);
    TEST_ASSERT_TRUE(str_eq_len(parsed.path, "/status", 7));
}

void test_url_complex_path()
{
    char url[] = "https://api.example.com/v1/users/123/posts";
    cwh_url_t parsed = {0};
    TEST_ASSERT_EQUAL(CWH_OK, cwh_parse_url(url, strlen(url), &parsed));
    TEST_ASSERT_TRUE(parsed.is_valid);
    TEST_ASSERT_TRUE(str_eq_len(parsed.host, "api.example.com", 15));
    TEST_ASSERT_TRUE(str_eq_len(parsed.path, "/v1/users/123/posts", 19));
}

void test_url_root_path()
{
    char url[] = "http://example.com/";
    cwh_url_t parsed = {0};
    TEST_ASSERT_EQUAL(CWH_OK, cwh_parse_url(url, strlen(url), &parsed));
    TEST_ASSERT_TRUE(parsed.is_valid);
    TEST_ASSERT_TRUE(str_eq_len(parsed.path, "/", 1));
}

void test_url_invalid_scheme()
{
    char url[] = "ftp://example.com";
    cwh_url_t parsed = {0};
    TEST_ASSERT_EQUAL(CWH_ERR_PARSE, cwh_parse_url(url, strlen(url), &parsed));
}

void test_url_invalid_port()
{
    char url[] = "http://example.com:99999/path";
    cwh_url_t parsed = {0};
    TEST_ASSERT_EQUAL(CWH_ERR_PARSE, cwh_parse_url(url, strlen(url), &parsed));
}

void test_url_invalid_port_chars()
{
    char url[] = "http://example.com:abc/path";
    cwh_url_t parsed = {0};
    TEST_ASSERT_EQUAL(CWH_ERR_PARSE, cwh_parse_url(url, strlen(url), &parsed));
}

void test_url_null_buffer()
{
    cwh_url_t parsed = {0};
    TEST_ASSERT_EQUAL(CWH_ERR_PARSE, cwh_parse_url(NULL, 100, &parsed));
}

void test_url_empty_buffer()
{
    char url[] = "";
    cwh_url_t parsed = {0};
    TEST_ASSERT_EQUAL(CWH_ERR_PARSE, cwh_parse_url(url, 0, &parsed));
}

void test_url_no_scheme_separator()
{
    char url[] = "http:example.com";
    cwh_url_t parsed = {0};
    TEST_ASSERT_EQUAL(CWH_ERR_PARSE, cwh_parse_url(url, strlen(url), &parsed));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_url_simple_http);
    RUN_TEST(test_url_https_with_path);
    RUN_TEST(test_url_custom_port);
    RUN_TEST(test_url_with_query);
    RUN_TEST(test_url_with_fragment);
    RUN_TEST(test_url_with_query_and_fragment);
    RUN_TEST(test_url_ip_address);
    RUN_TEST(test_url_complex_path);
    RUN_TEST(test_url_root_path);
    RUN_TEST(test_url_invalid_scheme);
    RUN_TEST(test_url_invalid_port);
    RUN_TEST(test_url_invalid_port_chars);
    RUN_TEST(test_url_null_buffer);
    RUN_TEST(test_url_empty_buffer);
    RUN_TEST(test_url_no_scheme_separator);
    return UNITY_END();
}
