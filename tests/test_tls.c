#include "unity.h"
#include "cwebhttp_tls.h"
#include <string.h>

void setUp(void)
{
    // Set up before each test
}

void tearDown(void)
{
    // Clean up after each test
}

void test_tls_is_available(void)
{
#if CWEBHTTP_ENABLE_TLS
    TEST_ASSERT_TRUE(cwh_tls_is_available());
#else
    TEST_ASSERT_FALSE(cwh_tls_is_available());
#endif
}

void test_tls_error_strings(void)
{
    TEST_ASSERT_NOT_NULL(cwh_tls_error_string(CWH_TLS_OK));
    TEST_ASSERT_NOT_NULL(cwh_tls_error_string(CWH_TLS_ERR_INIT));
    TEST_ASSERT_NOT_NULL(cwh_tls_error_string(CWH_TLS_ERR_HANDSHAKE));
    TEST_ASSERT_NOT_NULL(cwh_tls_error_string(CWH_TLS_ERR_CERT));
}

void test_tls_default_config(void)
{
    cwh_tls_config_t config = cwh_tls_config_default();
    TEST_ASSERT_TRUE(config.verify_peer);
    TEST_ASSERT_NULL(config.ca_cert_path);
    TEST_ASSERT_NULL(config.client_cert);
    TEST_ASSERT_NULL(config.client_key);
    TEST_ASSERT_EQUAL_INT(2, config.min_tls_version); // TLS 1.2
    TEST_ASSERT_EQUAL_INT(5000, config.timeout_ms);
}

#if CWEBHTTP_ENABLE_TLS

void test_tls_context_creation(void)
{
    cwh_tls_config_t config = cwh_tls_config_default();
    cwh_tls_context_t *ctx = cwh_tls_context_new(&config);
    TEST_ASSERT_NOT_NULL(ctx);
    cwh_tls_context_free(ctx);
}

void test_tls_context_null_config(void)
{
    cwh_tls_context_t *ctx = cwh_tls_context_new(NULL);
    TEST_ASSERT_NULL(ctx);
}

void test_tls_session_invalid_params(void)
{
    cwh_tls_config_t config = cwh_tls_config_default();
    cwh_tls_context_t *ctx = cwh_tls_context_new(&config);
    TEST_ASSERT_NOT_NULL(ctx);

    // Invalid socket
    cwh_tls_session_t *session = cwh_tls_session_new(ctx, -1, "example.com");
    TEST_ASSERT_NULL(session);

    // Invalid hostname
    session = cwh_tls_session_new(ctx, 1, NULL);
    TEST_ASSERT_NULL(session);

    // Invalid context
    session = cwh_tls_session_new(NULL, 1, "example.com");
    TEST_ASSERT_NULL(session);

    cwh_tls_context_free(ctx);
}

#endif // CWEBHTTP_ENABLE_TLS

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_tls_is_available);
    RUN_TEST(test_tls_error_strings);
    RUN_TEST(test_tls_default_config);

#if CWEBHTTP_ENABLE_TLS
    RUN_TEST(test_tls_context_creation);
    RUN_TEST(test_tls_context_null_config);
    RUN_TEST(test_tls_session_invalid_params);
#endif

    return UNITY_END();
}
