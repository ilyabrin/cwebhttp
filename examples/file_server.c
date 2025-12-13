#include "cwebhttp.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[])
{
    const char *port = "8080";
    const char *root_dir = "./www";

    // Parse command line arguments
    if (argc > 1)
        port = argv[1];
    if (argc > 2)
        root_dir = argv[2];

    printf("Starting static file server on port %s...\n", port);
    printf("Serving files from: %s\n", root_dir);

    // Create server
    cwh_server_t *srv = cwh_listen(port, 10);
    if (!srv)
    {
        fprintf(stderr, "Failed to create server on port %s\n", port);
        return 1;
    }

    printf("Server listening on http://localhost:%s\n", port);
    printf("Press Ctrl+C to stop the server.\n\n");

    // Register static file handler for all GET requests
    // The root_dir is passed as user_data to the handler
    // NULL pattern means match any path
    cwh_route(srv, "GET", NULL, cwh_serve_static, (void *)root_dir);

    // Run server (blocks forever)
    cwh_error_t err = cwh_run(srv);
    if (err != CWH_OK)
    {
        fprintf(stderr, "Server error: %d\n", err);
        cwh_free_server(srv);
        return 1;
    }

    // Cleanup (never reached in current implementation)
    cwh_free_server(srv);
    return 0;
}
