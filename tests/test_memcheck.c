// test_memcheck.c - Memory leak detection tests

#define CWH_MEMCHECK_ENABLED 1
#include "cwebhttp_memcheck.h"
#include "unity.h"
#include <stdlib.h>
#include <string.h>

void setUp(void)
{
    cwh_memcheck_reset();
}

void tearDown(void)
{
    // Each test should clean up its own memory
}

// Test 1: No leaks - proper cleanup
void test_memcheck_no_leaks(void)
{
    CWH_MEMCHECK_INIT();

    char *ptr1 = malloc(100);
    char *ptr2 = malloc(200);
    char *ptr3 = malloc(300);

    TEST_ASSERT_NOT_NULL(ptr1);
    TEST_ASSERT_NOT_NULL(ptr2);
    TEST_ASSERT_NOT_NULL(ptr3);

    free(ptr1);
    free(ptr2);
    free(ptr3);

    cwh_memcheck_stats_t stats = cwh_memcheck_get_stats();
    TEST_ASSERT_EQUAL(3, stats.total_allocations);
    TEST_ASSERT_EQUAL(3, stats.total_frees);
    TEST_ASSERT_EQUAL(0, stats.current_allocations);
    TEST_ASSERT_EQUAL(0, cwh_memcheck_has_leaks());
}

// Test 2: Detect single leak
void test_memcheck_single_leak(void)
{
    CWH_MEMCHECK_INIT();

    char *ptr = malloc(100);
    TEST_ASSERT_NOT_NULL(ptr);

    // Intentionally don't free
    cwh_memcheck_stats_t stats = cwh_memcheck_get_stats();
    TEST_ASSERT_EQUAL(1, stats.total_allocations);
    TEST_ASSERT_EQUAL(0, stats.total_frees);
    TEST_ASSERT_EQUAL(1, stats.current_allocations);
    TEST_ASSERT_EQUAL(1, cwh_memcheck_has_leaks());

    // Clean up for next test
    free(ptr);
}

// Test 3: Detect multiple leaks
void test_memcheck_multiple_leaks(void)
{
    CWH_MEMCHECK_INIT();

    char *ptr1 = malloc(100);
    char *ptr2 = malloc(200);
    char *ptr3 = malloc(300);

    TEST_ASSERT_NOT_NULL(ptr1);
    TEST_ASSERT_NOT_NULL(ptr2);
    TEST_ASSERT_NOT_NULL(ptr3);

    // Only free one
    free(ptr2);

    cwh_memcheck_stats_t stats = cwh_memcheck_get_stats();
    TEST_ASSERT_EQUAL(3, stats.total_allocations);
    TEST_ASSERT_EQUAL(1, stats.total_frees);
    TEST_ASSERT_EQUAL(2, stats.current_allocations);
    TEST_ASSERT_EQUAL(2, cwh_memcheck_has_leaks());

    // Clean up
    free(ptr1);
    free(ptr3);
}

// Test 4: Track peak memory usage
void test_memcheck_peak_memory(void)
{
    CWH_MEMCHECK_INIT();

    char *ptr1 = malloc(1000);
    char *ptr2 = malloc(2000);
    char *ptr3 = malloc(3000);

    cwh_memcheck_stats_t stats = cwh_memcheck_get_stats();
    TEST_ASSERT_EQUAL(6000, stats.current_bytes);
    TEST_ASSERT_EQUAL(6000, stats.peak_bytes);
    TEST_ASSERT_EQUAL(3, stats.peak_allocations);

    free(ptr2);

    stats = cwh_memcheck_get_stats();
    TEST_ASSERT_EQUAL(4000, stats.current_bytes);
    TEST_ASSERT_EQUAL(6000, stats.peak_bytes); // Peak should remain
    TEST_ASSERT_EQUAL(2, stats.current_allocations);
    TEST_ASSERT_EQUAL(3, stats.peak_allocations); // Peak should remain

    free(ptr1);
    free(ptr3);
}

// Test 5: Calloc tracking
void test_memcheck_calloc(void)
{
    CWH_MEMCHECK_INIT();

    char *ptr = calloc(10, 50);
    TEST_ASSERT_NOT_NULL(ptr);

    cwh_memcheck_stats_t stats = cwh_memcheck_get_stats();
    TEST_ASSERT_EQUAL(1, stats.total_allocations);
    TEST_ASSERT_EQUAL(500, stats.current_bytes);

    free(ptr);

    stats = cwh_memcheck_get_stats();
    TEST_ASSERT_EQUAL(0, stats.current_allocations);
}

// Test 6: Realloc tracking
void test_memcheck_realloc(void)
{
    CWH_MEMCHECK_INIT();

    char *ptr = malloc(100);
    TEST_ASSERT_NOT_NULL(ptr);

    cwh_memcheck_stats_t stats = cwh_memcheck_get_stats();
    TEST_ASSERT_EQUAL(1, stats.total_allocations);
    TEST_ASSERT_EQUAL(100, stats.current_bytes);

    ptr = realloc(ptr, 200);
    TEST_ASSERT_NOT_NULL(ptr);

    stats = cwh_memcheck_get_stats();
    TEST_ASSERT_EQUAL(2, stats.total_allocations); // realloc counts as new allocation
    TEST_ASSERT_EQUAL(200, stats.current_bytes);
    TEST_ASSERT_EQUAL(1, stats.current_allocations);

    free(ptr);

    stats = cwh_memcheck_get_stats();
    TEST_ASSERT_EQUAL(0, stats.current_allocations);
}

// Test 7: Get process memory info
void test_memcheck_process_memory(void)
{
    cwh_process_memory_t info;
    int ret = cwh_memcheck_get_process_memory(&info);

#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
    // Should succeed on supported platforms
    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_TRUE(info.rss > 0); // Should have some memory usage
#else
    // May fail on unsupported platforms
    (void)ret;
#endif
}

// Test 8: External tool detection
void test_memcheck_tool_detection(void)
{
    // These tests just verify the functions don't crash
    int is_valgrind = cwh_memcheck_is_valgrind();
    int is_asan = cwh_memcheck_is_asan();

    TEST_ASSERT_TRUE(is_valgrind == 0 || is_valgrind == 1);
    TEST_ASSERT_TRUE(is_asan == 0 || is_asan == 1);

#ifdef __SANITIZE_ADDRESS__
    TEST_ASSERT_EQUAL(1, is_asan);
#else
    TEST_ASSERT_EQUAL(0, is_asan);
#endif
}

// Test 9: Statistics accuracy
void test_memcheck_statistics(void)
{
    CWH_MEMCHECK_INIT();

    // Allocate 5 blocks
    char *ptrs[5];
    for (int i = 0; i < 5; i++)
    {
        ptrs[i] = malloc((i + 1) * 100);
    }

    cwh_memcheck_stats_t stats = cwh_memcheck_get_stats();
    TEST_ASSERT_EQUAL(5, stats.total_allocations);
    TEST_ASSERT_EQUAL(5, stats.current_allocations);
    TEST_ASSERT_EQUAL(1500, stats.total_bytes_allocated); // 100+200+300+400+500

    // Free 3 blocks
    for (int i = 0; i < 3; i++)
    {
        free(ptrs[i]);
    }

    stats = cwh_memcheck_get_stats();
    TEST_ASSERT_EQUAL(5, stats.total_allocations);
    TEST_ASSERT_EQUAL(3, stats.total_frees);
    TEST_ASSERT_EQUAL(2, stats.current_allocations);
    TEST_ASSERT_EQUAL(900, stats.current_bytes); // 400+500

    // Clean up
    free(ptrs[3]);
    free(ptrs[4]);
}

// Test 10: Reset functionality
void test_memcheck_reset(void)
{
    CWH_MEMCHECK_INIT();

    char *ptr = malloc(100);
    free(ptr);

    cwh_memcheck_stats_t stats = cwh_memcheck_get_stats();
    TEST_ASSERT_EQUAL(1, stats.total_allocations);
    TEST_ASSERT_EQUAL(1, stats.total_frees);

    cwh_memcheck_reset();

    stats = cwh_memcheck_get_stats();
    TEST_ASSERT_EQUAL(0, stats.total_allocations);
    TEST_ASSERT_EQUAL(0, stats.total_frees);
    TEST_ASSERT_EQUAL(0, stats.current_allocations);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_memcheck_no_leaks);
    RUN_TEST(test_memcheck_single_leak);
    RUN_TEST(test_memcheck_multiple_leaks);
    RUN_TEST(test_memcheck_peak_memory);
    RUN_TEST(test_memcheck_calloc);
    RUN_TEST(test_memcheck_realloc);
    RUN_TEST(test_memcheck_process_memory);
    RUN_TEST(test_memcheck_tool_detection);
    RUN_TEST(test_memcheck_statistics);
    RUN_TEST(test_memcheck_reset);

    return UNITY_END();
}
