/**
 * @file logging_demo.c
 * @brief Demonstrates the logging system capabilities
 */

#include "cwebhttp_log.h"
#include <stdio.h>
#include <stdlib.h>

/* Custom log handler example */
void custom_handler(
    cwh_log_level_t level,
    const char *file,
    int line,
    const char *func,
    const char *message,
    void *user_data)
{
    int *counter = (int *)user_data;
    (*counter)++;

    (void)func; /* Unused parameter */

    printf("[CUSTOM-%d] %s: %s (from %s:%d)\n",
           *counter,
           cwh_log_level_name(level),
           message,
           file,
           line);
}

void demo_basic_logging(void)
{
    printf("\n========================================\n");
    printf("Demo 1: Basic Logging\n");
    printf("========================================\n\n");

    cwh_log_init();

    CWH_LOG_DEBUG("This is a debug message");
    CWH_LOG_INFO("This is an info message");
    CWH_LOG_WARN("This is a warning message");
    CWH_LOG_ERROR("This is an error message");
}

void demo_log_levels(void)
{
    printf("\n========================================\n");
    printf("Demo 2: Log Level Filtering\n");
    printf("========================================\n\n");

    printf("Setting level to WARN...\n");
    cwh_log_set_level(CWH_LOG_WARN);

    CWH_LOG_DEBUG("You won't see this");
    CWH_LOG_INFO("You won't see this either");
    CWH_LOG_WARN("But you'll see this warning");
    CWH_LOG_ERROR("And this error");

    /* Reset to default */
    cwh_log_set_level(CWH_LOG_INFO);
}

void demo_custom_handler(void)
{
    printf("\n========================================\n");
    printf("Demo 3: Custom Handler\n");
    printf("========================================\n\n");

    int message_count = 0;
    cwh_log_set_handler(custom_handler, &message_count);

    CWH_LOG_INFO("Custom handler message 1");
    CWH_LOG_WARN("Custom handler message 2");
    CWH_LOG_ERROR("Custom handler message 3");

    printf("\nTotal messages logged: %d\n", message_count);

    /* Reset to default handler */
    cwh_log_reset_handler();
}

void demo_file_logging(void)
{
    printf("\n========================================\n");
    printf("Demo 4: File Logging\n");
    printf("========================================\n\n");

    if (cwh_log_set_file("app.log") == 0)
    {
        printf("Logging to file 'app.log'...\n");

        CWH_LOG_INFO("This message goes to the file");
        CWH_LOG_WARN("So does this warning");
        CWH_LOG_ERROR("And this error");

        cwh_log_close_file();
        printf("File logging complete. Check 'app.log'\n");
    }
    else
    {
        printf("Failed to open log file\n");
    }
}

void demo_formatting(void)
{
    printf("\n========================================\n");
    printf("Demo 5: Formatting Options\n");
    printf("========================================\n\n");

    printf("With timestamps and colors (default):\n");
    CWH_LOG_INFO("Formatted message");

    printf("\nWithout timestamps:\n");
    cwh_log_set_timestamps(0);
    CWH_LOG_INFO("No timestamp message");

    printf("\nWithout colors:\n");
    cwh_log_set_colors(0);
    CWH_LOG_WARN("Plain text warning");

    /* Reset defaults */
    cwh_log_set_timestamps(1);
    cwh_log_set_colors(1);
}

void demo_contextual_logging(void)
{
    printf("\n========================================\n");
    printf("Demo 6: Contextual Information\n");
    printf("========================================\n\n");

    int user_id = 42;
    const char *username = "john_doe";

    CWH_LOG_INFO("User login: id=%d, username=%s", user_id, username);
    CWH_LOG_WARN("High memory usage: %.2f MB", 1024.5);
    CWH_LOG_ERROR("Connection failed: errno=%d", 110);
}

void demo_color_output(void)
{
    printf("\n========================================\n");
    printf("Demo 7: Color Output\n");
    printf("========================================\n\n");

    cwh_log_set_colors(1); // Enable colors

    CWH_LOG_DEBUG("This is a debug message in color");
    CWH_LOG_INFO("This is an info message in color");
    CWH_LOG_WARN("This is a warning message in color");
    CWH_LOG_ERROR("This is an error message in color");
}

int main(void)
{
    printf("========================================\n");
    printf("CWebHTTP Logging System Demo\n");
    printf("========================================\n");

    demo_basic_logging();
    demo_log_levels();
    demo_custom_handler();
    demo_file_logging();
    demo_formatting();
    demo_contextual_logging();
    demo_color_output();

    printf("\n========================================\n");
    printf("Demo Complete!\n");
    printf("========================================\n\n");

    return 0;
}
