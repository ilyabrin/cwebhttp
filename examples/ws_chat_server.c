// WebSocket Chat Server Example
// A real-time chat room using WebSockets
//
// Usage: ./ws_chat_server
// Then connect with browser to: http://localhost:8080
// Open multiple browser tabs to chat between them!

#include "../include/cwebhttp_ws.h"
#include "../include/cwebhttp_async.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#endif

#define MAX_CLIENTS 100
#define PORT 8080

// Client connection info
typedef struct chat_client
{
    cwh_ws_conn_t *ws_conn;
    char username[32];
    time_t connected_at;
    struct chat_client *next;
} chat_client_t;

// Global client list
static chat_client_t *clients = NULL;
static int client_count = 0;

// === Utility Functions ===

static void broadcast_message(const char *message, chat_client_t *exclude)
{
    printf("[BROADCAST] %s\n", message);

    chat_client_t *client = clients;
    while (client)
    {
        if (client != exclude && client->ws_conn)
        {
            cwh_ws_send_text(client->ws_conn, message);
        }
        client = client->next;
    }
}

static void send_user_list(chat_client_t *to_client)
{
    char message[4096] = "{\"type\":\"users\",\"users\":[";
    bool first = true;

    chat_client_t *client = clients;
    while (client)
    {
        if (!first)
            strcat(message, ",");
        strcat(message, "\"");
        strcat(message, client->username);
        strcat(message, "\"");
        first = false;
        client = client->next;
    }

    strcat(message, "]}");
    cwh_ws_send_text(to_client->ws_conn, message);
}

static void send_system_message(const char *text)
{
    char message[1024];
    snprintf(message, sizeof(message),
             "{\"type\":\"system\",\"text\": \"%s\",\"time\":%ld}",
             text, (long)time(NULL));
    broadcast_message(message, NULL);
}

// === WebSocket Callbacks ===

static void on_ws_open(cwh_ws_conn_t *conn, void *user_data)
{
    printf("[WS] Connection opened\n");
}

static void on_ws_message(cwh_ws_conn_t *conn, const cwh_ws_message_t *msg, void *user_data)
{
    chat_client_t *client = (chat_client_t *)user_data;

    if (msg->opcode == CWH_WS_OP_TEXT)
    {
        // Parse JSON message (simple parser for demo)
        char *data = (char *)malloc(msg->len + 1);
        memcpy(data, msg->data, msg->len);
        data[msg->len] = '\0';

        printf("[MESSAGE from %s] %s\n", client->username, data);

        // Check message type
        if (strstr(data, "\"type\":\"join\""))
        {
            // Extract username
            char *username_start = strstr(data, "\"username\":\"");
            if (username_start)
            {
                username_start += 12;
                char *username_end = strchr(username_start, '"');
                if (username_end)
                {
                    size_t username_len = username_end - username_start;
                    if (username_len > 0 && username_len < sizeof(client->username))
                    {
                        memcpy(client->username, username_start, username_len);
                        client->username[username_len] = '\0';

                        // Announce user joined
                        char announcement[256];
                        snprintf(announcement, sizeof(announcement),
                                 "%s joined the chat", client->username);
                        send_system_message(announcement);

                        // Send user list to new client
                        send_user_list(client);
                    }
                }
            }
        }
        else if (strstr(data, "\"type\":\"message\""))
        {
            // Broadcast chat message
            char message[2048];
            snprintf(message, sizeof(message),
                     "{\"type\":\"message\",\"username\":\"%s\",\"text\":%s,\"time\":%ld}",
                     client->username,
                     strstr(data, "\"text\":") + 7,
                     (long)time(NULL));

            // Find the end of text field
            char *text_end = strstr(message + strlen(message) - 100, ",\"time\"");
            if (text_end)
            {
                memmove(text_end, strstr(message, ",\"time\""), strlen(strstr(message, ",\"time\"")) + 1);
            }

            broadcast_message(message, NULL);
        }

        free(data);
    }
}

static void on_ws_close(cwh_ws_conn_t *conn, uint16_t code, const char *reason, void *user_data)
{
    chat_client_t *client = (chat_client_t *)user_data;

    printf("[WS] Connection closed: %d - %s\n", code, reason);

    // Announce user left
    if (strlen(client->username) > 0)
    {
        char announcement[256];
        snprintf(announcement, sizeof(announcement),
                 "%s left the chat", client->username);
        send_system_message(announcement);
    }

    // Remove client from list
    chat_client_t **current = &clients;
    while (*current)
    {
        if (*current == client)
        {
            *current = client->next;
            client_count--;
            break;
        }
        current = &(*current)->next;
    }

    cwh_ws_conn_free(conn);
    free(client);
}

static void on_ws_error(cwh_ws_conn_t *conn, const char *error, void *user_data)
{
    printf("[WS ERROR] %s\n", error);
}

// === HTML Chat Client ===

static const char *html_client =
    "<!DOCTYPE html>\n"
    "<html>\n"
    "<head>\n"
    "    <title>WebSocket Chat</title>\n"
    "    <style>\n"
    "        body { font-family: Arial, sans-serif; max-width: 800px; margin: 50px auto; }\n"
    "        #chat { border: 1px solid #ccc; height: 400px; overflow-y: scroll; padding: 10px; margin-bottom: 10px; }\n"
    "        .message { margin: 5px 0; padding: 5px; }\n"
    "        .system { color: #666; font-style: italic; }\n"
    "        .user { color: #0066cc; font-weight: bold; }\n"
    "        #input { width: 70%; padding: 10px; }\n"
    "        #send { width: 25%; padding: 10px; }\n"
    "        #users { position: fixed; right: 20px; top: 50px; border: 1px solid #ccc; padding: 10px; }\n"
    "    </style>\n"
    "</head>\n"
    "<body>\n"
    "    <h1>WebSocket Chat Room</h1>\n"
    "    <div id=\"users\"><h3>Online Users</h3><div id=\"userList\"></div></div>\n"
    "    <div id=\"chat\"></div>\n"
    "    <input type=\"text\" id=\"input\" placeholder=\"Type a message...\" />\n"
    "    <button id=\"send\">Send</button>\n"
    "    <script>\n"
    "        const username = prompt('Enter your username:') || 'Anonymous';\n"
    "        const ws = new WebSocket('ws://localhost:8080/ws');\n"
    "        const chat = document.getElementById('chat');\n"
    "        const input = document.getElementById('input');\n"
    "        const send = document.getElementById('send');\n"
    "        const userList = document.getElementById('userList');\n"
    "        \n"
    "        ws.onopen = () => {\n"
    "            ws.send(JSON.stringify({type: 'join', username: username}));\n"
    "            addMessage('Connected to chat server', 'system');\n"
    "        };\n"
    "        \n"
    "        ws.onmessage = (event) => {\n"
    "            const msg = JSON.parse(event.data);\n"
    "            if (msg.type === 'system') {\n"
    "                addMessage(msg.text, 'system');\n"
    "            } else if (msg.type === 'message') {\n"
    "                addMessage(`${msg.username}: ${msg.text}`, 'user');\n"
    "            } else if (msg.type === 'users') {\n"
    "                updateUserList(msg.users);\n"
    "            }\n"
    "        };\n"
    "        \n"
    "        function addMessage(text, className) {\n"
    "            const div = document.createElement('div');\n"
    "            div.className = 'message ' + className;\n"
    "            div.textContent = text;\n"
    "            chat.appendChild(div);\n"
    "            chat.scrollTop = chat.scrollHeight;\n"
    "        }\n"
    "        \n"
    "        function updateUserList(users) {\n"
    "            userList.innerHTML = users.map(u => `<div>${u}</div>`).join('');\n"
    "        }\n"
    "        \n"
    "        function sendMessage() {\n"
    "            if (input.value.trim()) {\n"
    "                ws.send(JSON.stringify({type: 'message', text: input.value}));\n"
    "                input.value = '';\n"
    "            }\n"
    "        }\n"
    "        \n"
    "        send.onclick = sendMessage;\n"
    "        input.onkeypress = (e) => { if (e.key === 'Enter') sendMessage(); };\n"
    "    </script>\n"
    "</body>\n"
    "</html>";

// === HTTP Handler ===

static void handle_http_request(int client_fd, const char *request)
{
    if (strstr(request, "GET / "))
    {
        // Serve HTML client
        char response[8192];
        snprintf(response, sizeof(response),
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: text/html\r\n"
                 "Content-Length: %zu\r\n"
                 "\r\n"
                 "%s",
                 strlen(html_client), html_client);
        send(client_fd, response, strlen(response), 0);
#ifdef _WIN32
        closesocket(client_fd);
#else
        close(client_fd);
#endif
    }
    else if (strstr(request, "GET /ws") && cwh_ws_is_upgrade_request(request))
    {
        // WebSocket upgrade
        const char *key_header = strstr(request, "Sec-WebSocket-Key:");
        if (key_header)
        {
            key_header += 18;
            while (*key_header == ' ')
                key_header++;

            char key[64];
            sscanf(key_header, "%63s", key);

            char *handshake = cwh_ws_server_handshake(key);
            if (handshake)
            {
                send(client_fd, handshake, strlen(handshake), 0);
                free(handshake);

                // Create WebSocket connection
                cwh_ws_conn_t *ws_conn = cwh_ws_conn_new(client_fd, false);
                if (ws_conn)
                {
                    chat_client_t *client = (chat_client_t *)calloc(1, sizeof(chat_client_t));
                    client->ws_conn = ws_conn;
                    client->connected_at = time(NULL);
                    snprintf(client->username, sizeof(client->username), "User%d", ++client_count);

                    // Add to client list
                    client->next = clients;
                    clients = client;

                    printf("[HTTP] WebSocket connection upgraded for client %d\n", client_count);

                    // Set up callbacks
                    cwh_ws_callbacks_t callbacks = {
                        .on_open = on_ws_open,
                        .on_message = on_ws_message,
                        .on_close = on_ws_close,
                        .on_error = on_ws_error,
                        .user_data = client};

                    // Handle WebSocket communication (simple blocking for demo)
                    while (ws_conn->state == CWH_WS_STATE_OPEN)
                    {
                        if (cwh_ws_process(ws_conn, &callbacks) < 0)
                        {
                            break;
                        }
                    }
                }
            }
        }
    }
    else
    {
        // 404
        const char *response = "HTTP/1.1 404 Not Found\r\n\r\n";
        send(client_fd, response, strlen(response), 0);
#ifdef _WIN32
        closesocket(client_fd);
#else
        close(client_fd);
#endif
    }
}

// === Main Server ===

int main()
{
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        return 1;
    }

    if (listen(server_fd, 10) < 0)
    {
        perror("listen");
        return 1;
    }

    printf("=== WebSocket Chat Server ===\n");
    printf("Listening on http://localhost:%d\n", PORT);
    printf("Open your browser and navigate to http://localhost:%d\n\n", PORT);

    while (1)
    {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

        if (client_fd < 0)
        {
            perror("accept");
            continue;
        }

        printf("[ACCEPT] New connection from %s:%d\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // Read HTTP request
        char buffer[4096];
        int received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (received > 0)
        {
            buffer[received] = '\0';
            handle_http_request(client_fd, buffer);
        }
        else
        {
#ifdef _WIN32
            closesocket(client_fd);
#else
            close(client_fd);
#endif
        }
    }

#ifdef _WIN32
    closesocket(server_fd);
    WSACleanup();
#else
    close(server_fd);
#endif

    return 0;
}
