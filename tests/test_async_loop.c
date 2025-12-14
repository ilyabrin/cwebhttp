// test_async_loop.c - Event loop tests
// Tests for async event loop functionality

#include "cwebhttp_async.h"
#include "unity.h"
#include <stdio.h>
#include <string.h>

#ifndef _WIN32
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#else
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#endif

void setUp(void)
{
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
}

void tearDown(void)
{
#ifdef _WIN32
    WSACleanup();
#endif
}

// Test 1: Create and free event loop
void test_loop_create_free(void)
{
    cwh_loop_t *loop = cwh_loop_new();
    TEST_ASSERT_NOT_NULL(loop);

    const char *backend = cwh_loop_backend(loop);
    TEST_ASSERT_NOT_NULL(backend);
    printf("  Backend: %s\n", backend);

    cwh_loop_free(loop);
}

// Test 2: Backend name detection
void test_loop_backend(void)
{
    cwh_loop_t *loop = cwh_loop_new();
    TEST_ASSERT_NOT_NULL(loop);

    const char *backend = cwh_loop_backend(loop);
    TEST_ASSERT_NOT_NULL(backend);

    // Should be one of the known backends
#ifdef __linux__
    TEST_ASSERT_NOT_NULL(strstr(backend, "epoll"));
#elif defined(__APPLE__) || defined(__FreeBSD__)
    TEST_ASSERT_NOT_NULL(strstr(backend, "kqueue"));
#elif defined(_WIN32)
    TEST_ASSERT_NOT_NULL(strstr(backend, "IOCP"));
#else
    TEST_ASSERT_NOT_NULL(strstr(backend, "select"));
#endif

    cwh_loop_free(loop);
}

// Test 3: Set socket to non-blocking mode
void test_set_nonblocking(void)
{
#ifndef _WIN32 // Skip on Windows for now (needs different socket creation)
    int fds[2];
    int ret = pipe(fds);
    TEST_ASSERT_EQUAL(0, ret);

    ret = cwh_set_nonblocking(fds[0]);
    TEST_ASSERT_EQUAL(0, ret);

    ret = cwh_set_blocking(fds[0]);
    TEST_ASSERT_EQUAL(0, ret);

    close(fds[0]);
    close(fds[1]);
#endif
}

// Callback counter for event tests
static int callback_count = 0;

static void test_callback(cwh_loop_t *loop, int fd, int events, void *data)
{
    callback_count++;
    (void)fd;
    (void)events;
    (void)data;

    // Stop loop after first callback
    cwh_loop_stop(loop);
}

// Test 4: Add and remove file descriptor
void test_loop_add_del(void)
{
#ifndef _WIN32 // Unix pipe test
    cwh_loop_t *loop = cwh_loop_new();
    TEST_ASSERT_NOT_NULL(loop);

    int fds[2];
    int ret = pipe(fds);
    TEST_ASSERT_EQUAL(0, ret);

    // Set non-blocking
    cwh_set_nonblocking(fds[0]);

    // Add read event
    ret = cwh_loop_add(loop, fds[0], CWH_EVENT_READ, test_callback, NULL);
    TEST_ASSERT_EQUAL(0, ret);

    // Remove event
    ret = cwh_loop_del(loop, fds[0]);
    TEST_ASSERT_EQUAL(0, ret);

    close(fds[0]);
    close(fds[1]);
    cwh_loop_free(loop);
#endif
}

// Test 5: Event callback triggering
void test_loop_callback(void)
{
#ifndef _WIN32 // Unix pipe test
    cwh_loop_t *loop = cwh_loop_new();
    TEST_ASSERT_NOT_NULL(loop);

    int fds[2];
    int ret = pipe(fds);
    TEST_ASSERT_EQUAL(0, ret);

    // Set non-blocking
    cwh_set_nonblocking(fds[0]);

    // Write data to trigger read event
    const char *msg = "test";
    write(fds[1], msg, strlen(msg));

    // Add read event
    callback_count = 0;
    ret = cwh_loop_add(loop, fds[0], CWH_EVENT_READ, test_callback, NULL);
    TEST_ASSERT_EQUAL(0, ret);

    // Run loop once (should trigger callback)
    ret = cwh_loop_run_once(loop, 100); // 100ms timeout
    TEST_ASSERT_TRUE(ret >= 0);

    // Callback should have been called
    TEST_ASSERT_TRUE(callback_count > 0);

    close(fds[0]);
    close(fds[1]);
    cwh_loop_free(loop);
#endif
}

// Test 6: Modify events
void test_loop_modify(void)
{
#ifndef _WIN32 // Unix pipe test
    cwh_loop_t *loop = cwh_loop_new();
    TEST_ASSERT_NOT_NULL(loop);

    int fds[2];
    int ret = pipe(fds);
    TEST_ASSERT_EQUAL(0, ret);

    // Set non-blocking
    cwh_set_nonblocking(fds[0]);

    // Add read event
    ret = cwh_loop_add(loop, fds[0], CWH_EVENT_READ, test_callback, NULL);
    TEST_ASSERT_EQUAL(0, ret);

    // Modify to read+write
    ret = cwh_loop_mod(loop, fds[0], CWH_EVENT_READ | CWH_EVENT_WRITE);
    TEST_ASSERT_EQUAL(0, ret);

    // Remove event
    ret = cwh_loop_del(loop, fds[0]);
    TEST_ASSERT_EQUAL(0, ret);

    close(fds[0]);
    close(fds[1]);
    cwh_loop_free(loop);
#endif
}

int main(void)
{
    UNITY_BEGIN();

    printf("\n=== cwebhttp Async Event Loop Tests ===\n");
    printf("Testing event loop functionality\n\n");

    // Basic tests
    RUN_TEST(test_loop_create_free);
    RUN_TEST(test_loop_backend);
    RUN_TEST(test_set_nonblocking);

    // Event tests (Unix only for now)
#ifndef _WIN32
    RUN_TEST(test_loop_add_del);
    RUN_TEST(test_loop_callback);
    RUN_TEST(test_loop_modify);
#else
    printf("\nNote: Event tests skipped on Windows (epoll not available)\n");
#endif

    return UNITY_END();
}
