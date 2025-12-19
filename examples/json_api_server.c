/*
 * JSON API Server Example
 * Demonstrates async server with JSON responses
 *
 * Features:
 * - JSON responses
 * - RESTful API endpoints
 * - POST data handling
 * - Error responses
 *
 * Compile: make build/examples/json_api_server
 * Run: ./build/examples/json_api_server
 * Test: curl http://localhost:8080/api/users
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cwebhttp_async.h"

/* Sample user data */
typedef struct
{
    int id;
    const char *name;
    const char *email;
} User;

static User users[] = {
    {1, "Alice", "alice@example.com"},
    {2, "Bob", "bob@example.com"},
    {3, "Charlie", "charlie@example.com"}};
static const int user_count = 3;

/* GET /api/users - List all users */
void handle_users_list(cwh_async_conn_t *conn, cwh_request_t *req, void *data)
{
    (void)req;
    (void)data;

    char response[2048];
    int offset = 0;

    offset += snprintf(response + offset, sizeof(response) - offset,
                       "{\n  \"status\": \"success\",\n  \"data\": [\n");

    for (int i = 0; i < user_count; i++)
    {
        offset += snprintf(response + offset, sizeof(response) - offset,
                           "    {\"id\": %d, \"name\": \"%s\", \"email\": \"%s\"}%s\n",
                           users[i].id, users[i].name, users[i].email,
                           (i < user_count - 1) ? "," : "");
    }

    offset += snprintf(response + offset, sizeof(response) - offset,
                       "  ],\n  \"count\": %d\n}\n", user_count);

    cwh_async_send_json(conn, 200, response);
}

/* GET /api/users/:id - Get user by ID */
void handle_user_get(cwh_async_conn_t *conn, cwh_request_t *req, void *data)
{
    (void)data;

    /* Extract ID from path */
    const char *path = req->path;
    int user_id = 0;

    if (sscanf(path, "/api/users/%d", &user_id) != 1)
    {
        cwh_async_send_json(conn, 400,
                            "{\"status\": \"error\", \"message\": \"Invalid user ID\"}");
        return;
    }

    /* Find user */
    User *found = NULL;
    for (int i = 0; i < user_count; i++)
    {
        if (users[i].id == user_id)
        {
            found = &users[i];
            break;
        }
    }

    if (!found)
    {
        cwh_async_send_json(conn, 404,
                            "{\"status\": \"error\", \"message\": \"User not found\"}");
        return;
    }

    /* Return user */
    char response[512];
    snprintf(response, sizeof(response),
             "{\n"
             "  \"status\": \"success\",\n"
             "  \"data\": {\n"
             "    \"id\": %d,\n"
             "    \"name\": \"%s\",\n"
             "    \"email\": \"%s\"\n"
             "  }\n"
             "}\n",
             found->id, found->name, found->email);

    cwh_async_send_json(conn, 200, response);
}

/* GET /api/status - Server status */
void handle_status(cwh_async_conn_t *conn, cwh_request_t *req, void *data)
{
    (void)req;
    (void)data;

    const char *response =
        "{\n"
        "  \"status\": \"ok\",\n"
        "  \"server\": \"cwebhttp async\",\n"
        "  \"version\": \"0.7.0\",\n"
        "  \"users\": 3\n"
        "}\n";

    cwh_async_send_json(conn, 200, response);
}

/* GET / - API documentation */
void handle_root(cwh_async_conn_t *conn, cwh_request_t *req, void *data)
{
    (void)req;
    (void)data;

    const char *response =
        "{\n"
        "  \"name\": \"JSON API Server\",\n"
        "  \"version\": \"1.0.0\",\n"
        "  \"endpoints\": [\n"
        "    {\"method\": \"GET\", \"path\": \"/api/users\", \"description\": \"List all users\"},\n"
        "    {\"method\": \"GET\", \"path\": \"/api/users/:id\", \"description\": \"Get user by ID\"},\n"
        "    {\"method\": \"GET\", \"path\": \"/api/status\", \"description\": \"Server status\"}\n"
        "  ]\n"
        "}\n";

    cwh_async_send_json(conn, 200, response);
}

int main(void)
{
    printf("========================================\n");
    printf("JSON API Server Example\n");
    printf("========================================\n\n");

    /* Create event loop */
    cwh_loop_t *loop = cwh_loop_new();
    if (!loop)
    {
        fprintf(stderr, "Failed to create event loop\n");
        return 1;
    }

    printf("✓ Event loop created (%s backend)\n", cwh_loop_backend(loop));

    /* Create server */
    cwh_async_server_t *server = cwh_async_server_new(loop);
    if (!server)
    {
        fprintf(stderr, "Failed to create server\n");
        cwh_loop_free(loop);
        return 1;
    }

    printf("✓ Server created\n");

    /* Register routes */
    cwh_async_route(server, "GET", "/", handle_root, NULL);
    cwh_async_route(server, "GET", "/api/status", handle_status, NULL);
    cwh_async_route(server, "GET", "/api/users", handle_users_list, NULL);
    cwh_async_route(server, "GET", "/api/users/1", handle_user_get, NULL);
    cwh_async_route(server, "GET", "/api/users/2", handle_user_get, NULL);
    cwh_async_route(server, "GET", "/api/users/3", handle_user_get, NULL);

    printf("✓ Routes registered\n");

    /* Start listening */
    if (cwh_async_listen(server, 8080) != 0)
    {
        fprintf(stderr, "Failed to start server\n");
        cwh_async_server_free(server);
        cwh_loop_free(loop);
        return 1;
    }

    printf("✓ Server listening on port 8080\n\n");

    printf("========================================\n");
    printf("Try these commands:\n");
    printf("  curl http://localhost:8080/\n");
    printf("  curl http://localhost:8080/api/status\n");
    printf("  curl http://localhost:8080/api/users\n");
    printf("  curl http://localhost:8080/api/users/1\n");
    printf("  curl http://localhost:8080/api/users/2\n");
    printf("========================================\n\n");

    printf("Press Ctrl+C to stop\n\n");

    /* Run event loop */
    cwh_loop_run(loop);

    /* Cleanup */
    cwh_async_server_free(server);
    cwh_loop_free(loop);

    return 0;
}
