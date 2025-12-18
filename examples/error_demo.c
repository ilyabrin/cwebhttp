/*
 * CWebHTTP Error Handling Demo
 * Demonstrates the detailed error reporting system
 */

#include "../include/cwebhttp_error.h"
#include "../include/cwebhttp_log.h"
#include <stdio.h>
#include <string.h>

void print_separator(void)
{
    printf("\n========================================\n\n");
}

void demo_error_codes(void)
{
    printf("Error Code Demo:\n");
    printf("----------------\n\n");

    // Show various error codes
    cwh_error_code_t errors[] = {
        CWH_OK,
        CWH_ERR_ALLOC_OUT_OF_MEMORY,
        CWH_ERR_INVALID_ARGUMENT,
        CWH_ERR_NET_SOCKET_CREATE,
        CWH_ERR_NET_SOCKET_BIND,
        CWH_ERR_NET_SOCKET_LISTEN,
        CWH_ERR_NET_SOCKET_ACCEPT,
        CWH_ERR_NET_SEND,
        CWH_ERR_NET_RECV,
        CWH_ERR_NET_TIMEOUT,
        CWH_ERR_FILE_NOT_FOUND,
        CWH_ERR_PARSE_INVALID_REQUEST,
        CWH_ERR_FILE_READ,
        CWH_ERR_SSL_INIT,
        CWH_ERR_LOOP_INIT,
        CWH_ERR_INTERNAL};

    printf("%-30s %-15s %s\n", "Error Name", "Category", "Description");
    printf("%-30s %-15s %s\n", "----------", "--------", "-----------");

    for (size_t i = 0; i < sizeof(errors) / sizeof(errors[0]); i++)
    {
        printf("%-30s %-15s %s\n",
               cwh_error_string(errors[i]),
               cwh_error_category(errors[i]),
               cwh_error_string(errors[i]));
    }
}

void demo_error_context(void)
{
    printf("Error Context Demo:\n");
    printf("-------------------\n\n");

    // Example 1: Memory allocation error
    printf("1. Memory allocation failure:\n");
    cwh_error_t mem_error;
    cwh_error_init(&mem_error);
    CWH_ERROR_SET(&mem_error, CWH_ERR_ALLOC_OUT_OF_MEMORY, "Failed to allocate buffer");
    cwh_error_set_details(&mem_error, "Requested size: %d bytes", 1024000);
    cwh_error_print(&mem_error);
    printf("\n");

    // Example 2: Network error
    printf("2. Socket creation failure:\n");
    cwh_error_t net_error;
    cwh_error_init(&net_error);
    CWH_ERROR_SET(&net_error, CWH_ERR_NET_SOCKET_CREATE, "socket() failed");
    cwh_error_set_details(&net_error, "errno=98 (Address already in use)");
    cwh_error_print(&net_error);
    printf("\n");

    // Example 3: File error
    printf("3. File not found:\n");
    cwh_error_t file_error;
    cwh_error_init(&file_error);
    CWH_ERROR_SET(&file_error, CWH_ERR_FILE_NOT_FOUND, "Cannot open file");
    cwh_error_set_details(&file_error, "Path: /tmp/config.txt");
    cwh_error_print(&file_error);
    printf("\n");
}

void demo_thread_local_error(void)
{
    printf("Thread-Local Error Demo:\n");
    printf("------------------------\n\n");

    // Set thread-local error
    cwh_set_last_error(CWH_ERR_PARSE_INVALID_REQUEST, "Invalid HTTP request format");

    // Retrieve it
    cwh_error_t *last_error = cwh_get_last_error();
    printf("Last error code: %d\n", last_error->code);
    printf("Last error message: %s\n", last_error->message);
    printf("Category: %s\n", cwh_error_category(last_error->code));
}

void demo_error_categories(void)
{
    printf("Error Categories Demo:\n");
    printf("----------------------\n\n");

    cwh_error_code_t test_errors[] = {
        CWH_ERR_PARSE_INVALID_METHOD,
        CWH_ERR_NET_CONNECTION_CLOSED,
        CWH_ERR_ALLOC_BUFFER_TOO_SMALL,
        CWH_ERR_FILE_ACCESS_DENIED,
        CWH_ERR_SERVER_MAX_CONNECTIONS,
        CWH_ERR_LOOP_BACKEND_NOT_SUPPORTED};

    for (size_t i = 0; i < sizeof(test_errors) / sizeof(test_errors[0]); i++)
    {
        printf("Error: %-35s Category: %s\n",
               cwh_error_string(test_errors[i]),
               cwh_error_category(test_errors[i]));
    }
}

int main(void)
{
    printf("\n");
    printf("╔════════════════════════════════════════╗\n");
    printf("║  CWebHTTP Error Handling Demo         ║\n");
    printf("╚════════════════════════════════════════╝\n");

    print_separator();
    demo_error_codes();

    print_separator();
    demo_error_context();

    print_separator();
    demo_thread_local_error();

    print_separator();
    demo_error_categories();

    print_separator();
    printf("Demo completed successfully!\n\n");

    return 0;
}
