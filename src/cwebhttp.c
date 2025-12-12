#include "cwebhttp.h"
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h> // для Linux/macOS
#include <unistd.h>    // close

// Dummy connect (реальный потом с getaddrinfo)
cwh_conn_t *cwh_connect(const char *url, int timeout_ms)
{
    (void)timeout_ms; // ignore for now
    cwh_conn_t *conn = malloc(sizeof(cwh_conn_t));
    if (!conn)
        return NULL;
    conn->fd = -1; // dummy
    conn->host = strdup(url ? url : "localhost");
    conn->port = 80;
    return conn;
}

cwh_error_t cwh_send_req(cwh_conn_t *conn, cwh_method_t method, const char *path, const char **headers, const char *body, size_t body_len)
{
    (void)conn;
    (void)method;
    (void)path;
    (void)headers;
    (void)body;
    (void)body_len;
    if (conn->fd < 0)
        return CWH_ERR_NET;
    return CWH_OK; // dummy
}

cwh_error_t cwh_read_res(cwh_conn_t *conn, cwh_response_t *res)
{
    (void)conn;
    (void)res;
    res->status = 200;
    res->body = "Dummy response";
    res->body_len = strlen(res->body);
    return CWH_OK;
}

void cwh_close(cwh_conn_t *conn)
{
    if (conn)
    {
        free(conn->host);
        free(conn);
    }
}

// Dummy parse (реальный zero-alloc strstr-based потом)
cwh_error_t cwh_parse_req(const char *buf, size_t len, cwh_request_t *req)
{
    (void)len;
    if (!buf || strncmp(buf, "GET", 3) != 0)
        return CWH_ERR_PARSE;
    req->method_str = (char *)"GET";
    req->path = (char *)"/";
    req->is_valid = true;
    return CWH_OK;
}

cwh_error_t cwh_format_res(char *buf, size_t *out_len, const cwh_response_t *res)
{
    snprintf(buf, 1024, "HTTP/1.1 %d OK\r\nContent-Length: %zu\r\n\r\n", res->status, res->body_len);
    *out_len = strlen(buf) + res->body_len;
    strcat(buf, res->body);
    return CWH_OK;
}

// Сервер dummy
cwh_server_t *cwh_listen(const char *addr_port, int backlog)
{
    (void)backlog;
    cwh_server_t *srv = malloc(sizeof(cwh_server_t));
    if (!srv)
        return NULL;
    srv->sock = -1; // dummy bind
    return srv;
}

cwh_error_t cwh_route(cwh_server_t *srv, const char *method, const char *pattern, cwh_handler_t handler, void *user_data)
{
    (void)srv;
    (void)method;
    (void)pattern;
    (void)handler;
    (void)user_data;
    return CWH_OK;
}

cwh_error_t cwh_run(cwh_server_t *srv)
{
    (void)srv;
    printf("Dummy server running...\n");
    sleep(5); // dummy loop
    return CWH_OK;
}

void cwh_free_server(cwh_server_t *srv)
{
    free(srv);
}