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

void test_parse_get()
{
    char buf[] = "GET / HTTP/1.1\r\nHost: test\r\n\r\n";
    cwh_request_t req = {0};
    TEST_ASSERT_EQUAL(CWH_OK, cwh_parse_req(buf, sizeof(buf), &req));
    TEST_ASSERT_TRUE(req.is_valid);
    TEST_ASSERT_EQUAL_STRING("GET", req.method_str);
    TEST_ASSERT_EQUAL_STRING("/", req.path);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_parse_get);
    return UNITY_END();
}