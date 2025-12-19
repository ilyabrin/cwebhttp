#include "cwebhttp_error.h"
#include "cwebhttp_log.h"
#include <stdio.h>
#include <string.h>

// Simulate various error scenarios
void demo_parse_error(void)
{
    cwh_error_t error;
    cwh_error_init(&error);

    printf("\n=== Parse Error Demo ===\n");
    CWH_ERROR_SET(&error, CWH_ERR_PARSE_INVALID_METHOD, "Received unsupported HTTP method");
    cwh_error_set_details(&error, "Method: %s, Expected: GET, POST, PUT, DELETE", "PATCH");
    cwh_error_print(&error);
}

void demo_network_error(void)
{
    cwh_error_t error;
    cwh_error_init(&error);

    printf("\n=== Network Error Demo ===\n");
    CWH_ERROR_SET(&error, CWH_ERR_NET_SOCKET_CONNECT, "Failed to connect to remote server");
    cwh_error_set_details(&error, "Host: %s, Port: %d, Timeout: %dms", "example.com", 8080, 5000);
    cwh_error_print(&error);
}

void demo_file_error(void)
{
    cwh_error_t error;
    cwh_error_init(&error);

    printf("\n=== File Error Demo ===\n");
    CWH_ERROR_SET(&error, CWH_ERR_FILE_NOT_FOUND, "Static file not found");
    cwh_error_set_details(&error, "Path: %s, Requested by: %s", "/var/www/missing.html", "192.168.1.100");
    cwh_error_print(&error);
}

void demo_server_error(void)
{
    cwh_error_t error;
    cwh_error_init(&error);

    printf("\n=== Server Error Demo ===\n");
    CWH_ERROR_SET(&error, CWH_ERR_SERVER_MAX_CONNECTIONS, "Server cannot accept more connections");
    cwh_error_set_details(&error, "Current: %d, Max: %d, Consider increasing max_connections", 1000, 1000);
    cwh_error_print(&error);
}

void demo_eventloop_error(void)
{
    cwh_error_t error;
    cwh_error_init(&error);

    printf("\n=== Event Loop Error Demo ===\n");
    CWH_ERROR_SET(&error, CWH_ERR_LOOP_ADD_FD, "Failed to add file descriptor to event loop");
    cwh_error_set_details(&error, "FD: %d, Backend: %s, Events: %s", 42, "epoll", "READ|WRITE");
    cwh_error_print(&error);
}

void demo_error_categories(void)
{
    printf("\n=== Error Categories Demo ===\n");

    cwh_error_code_t codes[] = {
        CWH_ERR_PARSE_INVALID_REQUEST,
        CWH_ERR_NET_SOCKET_CREATE,
        CWH_ERR_ALLOC_FAILED,
        CWH_ERR_FILE_NOT_FOUND,
        CWH_ERR_SERVER_INIT,
        CWH_ERR_CLIENT_INIT,
        CWH_ERR_LOOP_INIT,
        CWH_ERR_SSL_HANDSHAKE,
        CWH_ERR_INVALID_ARGUMENT};

    for (size_t i = 0; i < sizeof(codes) / sizeof(codes[0]); i++)
    {
        printf("  [%s] Code: %d - %s\n",
               cwh_error_category(codes[i]),
               codes[i],
               cwh_error_string(codes[i]));
    }
}

// Simulate function that returns error
int simulate_operation(cwh_error_t *error)
{
    // Simulate a failure
    CWH_ERROR_RETURN(error, CWH_ERR_NET_TIMEOUT, "Operation timed out");
}

void demo_function_error(void)
{
    cwh_error_t error;
    cwh_error_init(&error);

    printf("\n=== Function Return Error Demo ===\n");
    int result = simulate_operation(&error);

    if (result != CWH_OK)
    {
        printf("Operation failed with code: %d\n", result);
        cwh_error_print(&error);
    }
}

void demo_last_error(void)
{
    printf("\n=== Thread-Local Last Error Demo ===\n");

    // Set last error
    cwh_set_last_error(CWH_ERR_ALLOC_OUT_OF_MEMORY, "Failed to allocate 1GB buffer");

    // Retrieve and print
    cwh_error_t *last_error = cwh_get_last_error();
    printf("Last error: %s (code: %d)\n", last_error->message, last_error->code);
}

void demo_integration_with_logging(void)
{
    printf("\n=== Integration with Logging Demo ===\n");

    cwh_error_t error;
    cwh_error_init(&error);

    // Log error messages
    CWH_ERROR_SET(&error, CWH_ERR_SERVER_START, "Failed to bind to port");
    cwh_error_set_details(&error, "Port: %d, Reason: Address already in use", 8080);

    CWH_LOG_ERROR("Server startup failed: %s", error.message);
    if (error.details[0] != '\0')
    {
        CWH_LOG_ERROR("  %s", error.details);
    }

    cwh_error_print(&error);
}

int main(void)
{
    printf("========================================\n");
    printf("  CWebHTTP Error Handling Demo\n");
    printf("========================================\n");

    // Initialize logging
    cwh_log_init();
    cwh_log_set_level(CWH_LOG_DEBUG);

    // Run demos
    demo_parse_error();
    demo_network_error();
    demo_file_error();
    demo_server_error();
    demo_eventloop_error();
    demo_error_categories();
    demo_function_error();
    demo_last_error();
    demo_integration_with_logging();

    printf("\n========================================\n");
    printf("  Demo completed successfully!\n");
    printf("========================================\n");

    return 0;
}
