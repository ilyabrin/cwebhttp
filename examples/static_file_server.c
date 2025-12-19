/*
 * Static File Server Example
 * Serves files from the www/ directory
 *
 * Features:
 * - Async file serving
 * - Content-Type detection
 * - Directory listing
 * - 404 handling
 *
 * Compile: make build/examples/static_file_server
 * Run: ./build/examples/static_file_server
 * Test: curl http://localhost:8080/index.html
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "cwebhttp_async.h"

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define stat _stat
#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
#endif
#ifndef S_ISREG
#define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
#endif
#else
#include <dirent.h>
#include <unistd.h>
#endif

#define MAX_PATH_LEN 512
#define MAX_FILE_SIZE (10 * 1024 * 1024) /* 10MB */
#define WWW_ROOT "www"

/* Get content type from file extension */
const char *get_content_type(const char *path)
{
    const char *ext = strrchr(path, '.');
    if (!ext)
        return "application/octet-stream";

    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0)
        return "text/html";
    if (strcmp(ext, ".css") == 0)
        return "text/css";
    if (strcmp(ext, ".js") == 0)
        return "application/javascript";
    if (strcmp(ext, ".json") == 0)
        return "application/json";
    if (strcmp(ext, ".png") == 0)
        return "image/png";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0)
        return "image/jpeg";
    if (strcmp(ext, ".gif") == 0)
        return "image/gif";
    if (strcmp(ext, ".svg") == 0)
        return "image/svg+xml";
    if (strcmp(ext, ".txt") == 0)
        return "text/plain";
    if (strcmp(ext, ".pdf") == 0)
        return "application/pdf";

    return "application/octet-stream";
}

/* Check if path is safe (no directory traversal) */
int is_safe_path(const char *path)
{
    /* Check for directory traversal attempts */
    if (strstr(path, "..") != NULL)
        return 0;
    if (strstr(path, "//") != NULL)
        return 0;
    return 1;
}

/* Serve a file */
void serve_file(cwh_async_conn_t *conn, const char *filepath)
{
    FILE *f = fopen(filepath, "rb");
    if (!f)
    {
        cwh_async_send_status(conn, 404, "File not found");
        return;
    }

    /* Get file size */
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size > MAX_FILE_SIZE)
    {
        fclose(f);
        cwh_async_send_status(conn, 413, "File too large");
        return;
    }

    /* Read file */
    char *content = malloc(size + 1);
    if (!content)
    {
        fclose(f);
        cwh_async_send_status(conn, 500, "Out of memory");
        return;
    }

    size_t read_size = fread(content, 1, size, f);
    fclose(f);

    if ((long)read_size != size)
    {
        free(content);
        cwh_async_send_status(conn, 500, "Failed to read file");
        return;
    }

    content[size] = '\0';

    /* Get content type */
    const char *content_type = get_content_type(filepath);

    /* Build response */
    char headers[256];
    snprintf(headers, sizeof(headers),
             "Content-Type: %s\r\n"
             "Content-Length: %ld\r\n",
             content_type, size);

    cwh_async_send_response(conn, 200, headers, content, size);

    free(content);
}

/* Directory listing (simple HTML) */
void serve_directory(cwh_async_conn_t *conn, const char *dirpath, const char *url_path)
{
    (void)dirpath; // Unused on Windows
    char html[8192];
    int offset = 0;

    offset += snprintf(html + offset, sizeof(html) - offset,
                       "<!DOCTYPE html>\n"
                       "<html>\n"
                       "<head>\n"
                       "  <title>Directory: %s</title>\n"
                       "  <style>\n"
                       "    body { font-family: Arial, sans-serif; margin: 40px; }\n"
                       "    h1 { color: #333; }\n"
                       "    ul { list-style: none; padding: 0; }\n"
                       "    li { padding: 8px; border-bottom: 1px solid #eee; }\n"
                       "    a { color: #0066cc; text-decoration: none; }\n"
                       "    a:hover { text-decoration: underline; }\n"
                       "  </style>\n"
                       "</head>\n"
                       "<body>\n"
                       "  <h1>Directory: %s</h1>\n"
                       "  <ul>\n",
                       url_path, url_path);

    /* Add parent directory link */
    if (strcmp(url_path, "/") != 0)
    {
        offset += snprintf(html + offset, sizeof(html) - offset,
                           "    <li><a href=\"..\">[Parent Directory]</a></li>\n");
    }

#ifndef _WIN32
    /* List directory contents (Unix) */
    DIR *dir = opendir(dirpath);
    if (dir)
    {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL)
        {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            {
                continue;
            }

            offset += snprintf(html + offset, sizeof(html) - offset,
                               "    <li><a href=\"%s%s\">%s</a></li>\n",
                               strcmp(url_path, "/") == 0 ? "" : url_path,
                               entry->d_name,
                               entry->d_name);
        }
        closedir(dir);
    }
#endif

    offset += snprintf(html + offset, sizeof(html) - offset,
                       "  </ul>\n"
                       "</body>\n"
                       "</html>\n");

    cwh_async_send_response(conn, 200, "Content-Type: text/html\r\n", html, offset);
}

/* Main file handler */
void handle_file_request(cwh_async_conn_t *conn, cwh_request_t *req, void *data)
{
    (void)data;

    const char *url_path = req->path;

    /* Security check */
    if (!is_safe_path(url_path))
    {
        cwh_async_send_status(conn, 403, "Forbidden");
        return;
    }

    /* Build filesystem path */
    char filepath[MAX_PATH_LEN];
    snprintf(filepath, sizeof(filepath), "%s%s", WWW_ROOT, url_path);

    /* Handle root path */
    if (strcmp(url_path, "/") == 0)
    {
        snprintf(filepath, sizeof(filepath), "%s/index.html", WWW_ROOT);
    }

    /* Check if file/directory exists */
    struct stat st;
    if (stat(filepath, &st) != 0)
    {
        cwh_async_send_status(conn, 404, "Not found");
        return;
    }

    /* Serve directory or file */
    if (S_ISDIR(st.st_mode))
    {
        /* Check for index.html in directory */
        char index_path[MAX_PATH_LEN + 20];
        snprintf(index_path, sizeof(index_path), "%s/index.html", filepath);

        struct stat index_st;
        if (stat(index_path, &index_st) == 0 && S_ISREG(index_st.st_mode))
        {
            serve_file(conn, index_path);
        }
        else
        {
            serve_directory(conn, filepath, url_path);
        }
    }
    else if (S_ISREG(st.st_mode))
    {
        serve_file(conn, filepath);
    }
    else
    {
        cwh_async_send_status(conn, 403, "Forbidden");
    }
}

int main(void)
{
    printf("========================================\n");
    printf("Static File Server Example\n");
    printf("========================================\n\n");

    /* Check if www directory exists */
    struct stat st;
    if (stat(WWW_ROOT, &st) != 0 || !S_ISDIR(st.st_mode))
    {
        fprintf(stderr, "Error: '%s' directory not found!\n", WWW_ROOT);
        fprintf(stderr, "Please create it and add some files.\n");
        return 1;
    }

    /* Create event loop */
    cwh_loop_t *loop = cwh_loop_new();
    if (!loop)
    {
        fprintf(stderr, "Failed to create event loop\n");
        return 1;
    }

    printf("✓ Event loop created\n");

    /* Create server */
    cwh_async_server_t *server = cwh_async_server_new(loop);
    if (!server)
    {
        fprintf(stderr, "Failed to create server\n");
        cwh_loop_free(loop);
        return 1;
    }

    printf("✓ Server created\n");

    /* Register catch-all route */
    cwh_async_route(server, "GET", "*", handle_file_request, NULL);

    printf("✓ Routes registered\n");

    /* Start listening */
    if (cwh_async_listen(server, 8080) != 0)
    {
        fprintf(stderr, "Failed to start server\n");
        cwh_async_server_free(server);
        cwh_loop_free(loop);
        return 1;
    }

    printf("✓ Server listening on port 8080\n");
    printf("✓ Serving files from: %s/\n\n", WWW_ROOT);

    printf("========================================\n");
    printf("Try: http://localhost:8080/\n");
    printf("Press Ctrl+C to stop\n");
    printf("========================================\n\n");

    /* Run event loop */
    cwh_loop_run(loop);

    /* Cleanup */
    cwh_async_server_free(server);
    cwh_loop_free(loop);

    return 0;
}
