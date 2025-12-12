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

// Test decoding a simple chunked body
void test_decode_chunked_simple()
{
    // Simple chunked body: "Hello" in one chunk
    const char *chunked = "5\r\nHello\r\n0\r\n\r\n";
    char output[1024] = {0};
    size_t out_len = 0;

    cwh_error_t err = cwh_decode_chunked(chunked, strlen(chunked), output, &out_len);

    TEST_ASSERT_EQUAL(CWH_OK, err);
    TEST_ASSERT_EQUAL(5, out_len);
    TEST_ASSERT_EQUAL_MEMORY("Hello", output, 5);
}

// Test decoding multiple chunks
void test_decode_chunked_multiple()
{
    // Two chunks: "Hello" + " World"
    const char *chunked = "5\r\nHello\r\n6\r\n World\r\n0\r\n\r\n";
    char output[1024] = {0};
    size_t out_len = 0;

    cwh_error_t err = cwh_decode_chunked(chunked, strlen(chunked), output, &out_len);

    TEST_ASSERT_EQUAL(CWH_OK, err);
    TEST_ASSERT_EQUAL(11, out_len);
    TEST_ASSERT_EQUAL_MEMORY("Hello World", output, 11);
}

// Test decoding with uppercase hex
void test_decode_chunked_uppercase_hex()
{
    // Chunk size in uppercase hex
    const char *chunked = "A\r\n0123456789\r\n0\r\n\r\n";
    char output[1024] = {0};
    size_t out_len = 0;

    cwh_error_t err = cwh_decode_chunked(chunked, strlen(chunked), output, &out_len);

    TEST_ASSERT_EQUAL(CWH_OK, err);
    TEST_ASSERT_EQUAL(10, out_len);
    TEST_ASSERT_EQUAL_MEMORY("0123456789", output, 10);
}

// Test decoding with chunk extensions
void test_decode_chunked_with_extensions()
{
    // Chunk with extension (should be ignored)
    const char *chunked = "5;name=value\r\nHello\r\n0\r\n\r\n";
    char output[1024] = {0};
    size_t out_len = 0;

    cwh_error_t err = cwh_decode_chunked(chunked, strlen(chunked), output, &out_len);

    TEST_ASSERT_EQUAL(CWH_OK, err);
    TEST_ASSERT_EQUAL(5, out_len);
    TEST_ASSERT_EQUAL_MEMORY("Hello", output, 5);
}

// Test decoding large chunk (hex > single digit)
void test_decode_chunked_large()
{
    // 256 bytes (0x100 in hex)
    char chunked[512];
    char expected[256];
    memset(expected, 'A', 256);

    // Build chunked encoding: "100\r\n" + 256*'A' + "\r\n0\r\n\r\n"
    strcpy(chunked, "100\r\n");
    memcpy(chunked + 5, expected, 256);
    strcpy(chunked + 5 + 256, "\r\n0\r\n\r\n");

    char output[1024] = {0};
    size_t out_len = 0;

    cwh_error_t err = cwh_decode_chunked(chunked, strlen(chunked), output, &out_len);

    TEST_ASSERT_EQUAL(CWH_OK, err);
    TEST_ASSERT_EQUAL(256, out_len);
    TEST_ASSERT_EQUAL_MEMORY(expected, output, 256);
}

// Test decoding empty body (just terminator)
void test_decode_chunked_empty()
{
    const char *chunked = "0\r\n\r\n";
    char output[1024] = {0};
    size_t out_len = 0;

    cwh_error_t err = cwh_decode_chunked(chunked, strlen(chunked), output, &out_len);

    TEST_ASSERT_EQUAL(CWH_OK, err);
    TEST_ASSERT_EQUAL(0, out_len);
}

// Test decoding with binary data
void test_decode_chunked_binary()
{
    // Binary data including null bytes
    const char binary_data[] = {0x00, 0x01, 0x02, 0x03, 0xFF};
    char chunked[64];
    strcpy(chunked, "5\r\n");
    memcpy(chunked + 3, binary_data, 5);
    strcpy(chunked + 8, "\r\n0\r\n\r\n");

    char output[1024] = {0};
    size_t out_len = 0;

    cwh_error_t err = cwh_decode_chunked(chunked, 15, output, &out_len);

    TEST_ASSERT_EQUAL(CWH_OK, err);
    TEST_ASSERT_EQUAL(5, out_len);
    TEST_ASSERT_EQUAL_MEMORY(binary_data, output, 5);
}

// Test encoding simple body
void test_encode_chunked_simple()
{
    const char *body = "Hello World";
    char output[1024] = {0};
    size_t out_len = 0;

    cwh_error_t err = cwh_encode_chunked(body, strlen(body), output, &out_len);

    TEST_ASSERT_EQUAL(CWH_OK, err);
    TEST_ASSERT_GREATER_THAN(0, out_len);

    // Verify we can decode it back
    char decoded[1024] = {0};
    size_t decoded_len = 0;
    err = cwh_decode_chunked(output, out_len, decoded, &decoded_len);

    TEST_ASSERT_EQUAL(CWH_OK, err);
    TEST_ASSERT_EQUAL(strlen(body), decoded_len);
    TEST_ASSERT_EQUAL_MEMORY(body, decoded, decoded_len);
}

// Test encoding large body (multiple chunks)
void test_encode_chunked_large()
{
    // 10KB body (will be split into multiple 4KB chunks)
    char body[10240];
    memset(body, 'X', sizeof(body));

    char output[20480] = {0};
    size_t out_len = 0;

    cwh_error_t err = cwh_encode_chunked(body, sizeof(body), output, &out_len);

    TEST_ASSERT_EQUAL(CWH_OK, err);
    TEST_ASSERT_GREATER_THAN(sizeof(body), out_len); // Chunked is larger than original

    // Verify we can decode it back
    char decoded[10240] = {0};
    size_t decoded_len = 0;
    err = cwh_decode_chunked(output, out_len, decoded, &decoded_len);

    TEST_ASSERT_EQUAL(CWH_OK, err);
    TEST_ASSERT_EQUAL(sizeof(body), decoded_len);
    TEST_ASSERT_EQUAL_MEMORY(body, decoded, decoded_len);
}

// Test encoding empty body
void test_encode_chunked_empty()
{
    const char *body = "";
    char output[1024] = {0};
    size_t out_len = 0;

    cwh_error_t err = cwh_encode_chunked(body, 0, output, &out_len);

    TEST_ASSERT_EQUAL(CWH_OK, err);
    // Should just be "0\r\n\r\n"
    TEST_ASSERT_EQUAL_STRING("0\r\n\r\n", output);
}

// Test round-trip encoding and decoding
void test_chunked_roundtrip()
{
    const char *original = "The quick brown fox jumps over the lazy dog. 1234567890!@#$%^&*()";
    char encoded[1024] = {0};
    char decoded[1024] = {0};
    size_t encoded_len = 0;
    size_t decoded_len = 0;

    // Encode
    cwh_error_t err = cwh_encode_chunked(original, strlen(original), encoded, &encoded_len);
    TEST_ASSERT_EQUAL(CWH_OK, err);

    // Decode
    err = cwh_decode_chunked(encoded, encoded_len, decoded, &decoded_len);
    TEST_ASSERT_EQUAL(CWH_OK, err);

    // Verify
    TEST_ASSERT_EQUAL(strlen(original), decoded_len);
    TEST_ASSERT_EQUAL_MEMORY(original, decoded, decoded_len);
}

// Test invalid inputs - NULL buffer
void test_decode_chunked_null_buffer()
{
    char output[1024];
    size_t out_len = 0;

    cwh_error_t err = cwh_decode_chunked(NULL, 100, output, &out_len);
    TEST_ASSERT_EQUAL(CWH_ERR_PARSE, err);
}

// Test invalid inputs - malformed chunk size
void test_decode_chunked_invalid_hex()
{
    const char *chunked = "XYZ\r\nHello\r\n0\r\n\r\n";
    char output[1024] = {0};
    size_t out_len = 0;

    cwh_error_t err = cwh_decode_chunked(chunked, strlen(chunked), output, &out_len);
    TEST_ASSERT_EQUAL(CWH_ERR_PARSE, err);
}

// Test invalid inputs - missing CRLF after chunk size
void test_decode_chunked_missing_crlf()
{
    const char *chunked = "5Hello\r\n0\r\n\r\n";
    char output[1024] = {0};
    size_t out_len = 0;

    cwh_error_t err = cwh_decode_chunked(chunked, strlen(chunked), output, &out_len);
    TEST_ASSERT_EQUAL(CWH_ERR_PARSE, err);
}

// Test invalid inputs - chunk size exceeds data
void test_decode_chunked_size_exceeds_data()
{
    const char *chunked = "10\r\nHello\r\n0\r\n\r\n"; // Says 16 bytes, but only 5 bytes available
    char output[1024] = {0};
    size_t out_len = 0;

    cwh_error_t err = cwh_decode_chunked(chunked, strlen(chunked), output, &out_len);
    TEST_ASSERT_EQUAL(CWH_ERR_PARSE, err);
}

// Test automatic chunked decoding in response parser
void test_response_parser_auto_decode_chunked()
{
    // HTTP response with chunked body
    char response[] = "HTTP/1.1 200 OK\r\n"
                      "Transfer-Encoding: chunked\r\n"
                      "Content-Type: text/plain\r\n"
                      "\r\n"
                      "5\r\nHello\r\n6\r\n World\r\n0\r\n\r\n";

    cwh_response_t res = {0};
    cwh_error_t err = cwh_parse_res(response, strlen(response), &res);

    TEST_ASSERT_EQUAL(CWH_OK, err);
    TEST_ASSERT_EQUAL(200, res.status);

    // Body should be automatically decoded
    TEST_ASSERT_EQUAL(11, res.body_len);
    TEST_ASSERT_EQUAL_MEMORY("Hello World", res.body, 11);
}

int main()
{
    UNITY_BEGIN();

    // Decoding tests
    RUN_TEST(test_decode_chunked_simple);
    RUN_TEST(test_decode_chunked_multiple);
    RUN_TEST(test_decode_chunked_uppercase_hex);
    RUN_TEST(test_decode_chunked_with_extensions);
    RUN_TEST(test_decode_chunked_large);
    RUN_TEST(test_decode_chunked_empty);
    RUN_TEST(test_decode_chunked_binary);

    // Encoding tests
    RUN_TEST(test_encode_chunked_simple);
    RUN_TEST(test_encode_chunked_large);
    RUN_TEST(test_encode_chunked_empty);

    // Round-trip tests
    RUN_TEST(test_chunked_roundtrip);

    // Error handling tests
    RUN_TEST(test_decode_chunked_null_buffer);
    RUN_TEST(test_decode_chunked_invalid_hex);
    RUN_TEST(test_decode_chunked_missing_crlf);
    RUN_TEST(test_decode_chunked_size_exceeds_data);

    // Integration test
    RUN_TEST(test_response_parser_auto_decode_chunked);

    return UNITY_END();
}
