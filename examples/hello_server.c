#include "cwebhttp.h"
#include <stdio.h>
#include <string.h>

// Handler for GET /
cwh_error_t handle_root(cwh_request_t *req, cwh_conn_t *conn, void *user_data)
{
    (void)req;
    (void)user_data;

    const char *html =
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head><title>cwebhttp Server</title></head>\n"
        "<body>\n"
        "<h1>Hello from cwebhttp!</h1>\n"
        "<p>This is a minimal HTTP/1.1 server built with cwebhttp.</p>\n"
        "<ul>\n"
        "<li><a href=\"/\">Home</a></li>\n"
        "<li><a href=\"/api/info\">API Info</a></li>\n"
        "<li><a href=\"/api/echo\">Echo Test (POST)</a></li>\n"
        "</ul>\n"
        "</body>\n"
        "</html>";

    return cwh_send_response(conn, 200, "text/html", html, strlen(html));
}

// Handler for GET /api/info
cwh_error_t handle_api_info(cwh_request_t *req, cwh_conn_t *conn, void *user_data)
{
    (void)req;
    (void)user_data;

    const char *json =
        "{\n"
        "  \"server\": \"cwebhttp\",\n"
        "  \"version\": \"0.3.0\",\n"
        "  \"method\": \"GET\",\n"
        "  \"endpoint\": \"/api/info\"\n"
        "}";

    return cwh_send_response(conn, 200, "application/json", json, strlen(json));
}

// Handler for POST /api/echo
cwh_error_t handle_api_echo(cwh_request_t *req, cwh_conn_t *conn, void *user_data)
{
    (void)user_data;

    // Echo back the request body
    if (req->body && req->body_len > 0)
    {
        return cwh_send_response(conn, 200, "text/plain", req->body, req->body_len);
    }
    else
    {
        return cwh_send_status(conn, 400, "No body in POST request");
    }
}

// Catch-all handler for unmatched routes
cwh_error_t handle_404(cwh_request_t *req, cwh_conn_t *conn, void *user_data)
{
    (void)req;
    (void)user_data;

    const char *html =
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head><title>404 Not Found</title></head>\n"
        "<body>\n"
        "<h1>404 Not Found</h1>\n"
        "<p>The requested page was not found.</p>\n"
        "</body>\n"
        "</html>";

    return cwh_send_response(conn, 404, "text/html", html, strlen(html));
}

int main(int argc, char *argv[])
{
    const char *port = "8080";

    // Allow custom port via command line
    if (argc > 1)
    {
        port = argv[1];
    }

    printf("Starting cwebhttp server on port %s...\n", port);

    // Create server
    cwh_server_t *srv = cwh_listen(port, 10);
    if (!srv)
    {
        fprintf(stderr, "Failed to create server on port %s\n", port);
        return 1;
    }

    printf("Server listening on http://localhost:%s\n", port);

    // Register routes
    cwh_route(srv, "GET", "/", handle_root, NULL);
    cwh_route(srv, "GET", "/api/info", handle_api_info, NULL);
    cwh_route(srv, "POST", "/api/echo", handle_api_echo, NULL);

    printf("\nAvailable endpoints:\n");
    printf("  GET  http://localhost:%s/\n", port);
    printf("  GET  http://localhost:%s/api/info\n", port);
    printf("  POST http://localhost:%s/api/echo\n", port);
    printf("\nPress Ctrl+C to stop the server.\n\n");

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
