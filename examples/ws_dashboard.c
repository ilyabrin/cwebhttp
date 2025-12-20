// WebSocket Real-Time Dashboard
// Streams system metrics (CPU, memory, connections) to browser clients
//
// Usage: ./ws_dashboard
// Then connect with browser to: http://localhost:8081

#include "../include/cwebhttp_ws.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "psapi.lib")
typedef int socklen_t;
#else
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#endif

#define PORT 8081
#define UPDATE_INTERVAL_MS 1000

// Dashboard client
typedef struct dashboard_client
{
    cwh_ws_conn_t *ws_conn;
    time_t connected_at;
    struct dashboard_client *next;
} dashboard_client_t;

static dashboard_client_t *clients = NULL;
static int client_count = 0;

// === System Metrics ===

typedef struct
{
    double cpu_usage;
    double memory_usage_mb;
    double memory_total_mb;
    int active_connections;
    time_t uptime_seconds;
} system_metrics_t;

static time_t server_start_time;

static void get_system_metrics(system_metrics_t *metrics)
{
    metrics->active_connections = client_count;
    metrics->uptime_seconds = time(NULL) - server_start_time;

#ifdef _WIN32
    // Windows system info
    MEMORYSTATUSEX mem_info;
    mem_info.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&mem_info);

    metrics->memory_total_mb = mem_info.ullTotalPhys / (1024.0 * 1024.0);
    metrics->memory_usage_mb = (mem_info.ullTotalPhys - mem_info.ullAvailPhys) / (1024.0 * 1024.0);

    // Simplified CPU usage (random for demo)
    metrics->cpu_usage = 10.0 + (rand() % 40);
#else
    // Linux system info
    struct sysinfo info;
    sysinfo(&info);

    metrics->memory_total_mb = info.totalram / (1024.0 * 1024.0);
    metrics->memory_usage_mb = (info.totalram - info.freeram) / (1024.0 * 1024.0);

    // Simplified CPU usage (random for demo)
    metrics->cpu_usage = 10.0 + (rand() % 40);
#endif
}

// === WebSocket Communication ===

static void broadcast_metrics()
{
    system_metrics_t metrics;
    get_system_metrics(&metrics);

    char message[1024];
    snprintf(message, sizeof(message),
             "{\"type\":\"metrics\","
             "\"cpu\":%.2f,"
             "\"memory_used\":%.2f,"
             "\"memory_total\":%.2f,"
             "\"connections\":%d,"
             "\"uptime\":%ld,"
             "\"timestamp\":%ld}",
             metrics.cpu_usage,
             metrics.memory_usage_mb,
             metrics.memory_total_mb,
             metrics.active_connections,
             (long)metrics.uptime_seconds,
             (long)time(NULL));

    dashboard_client_t *client = clients;
    while (client)
    {
        if (client->ws_conn && client->ws_conn->state == CWH_WS_STATE_OPEN)
        {
            cwh_ws_send_text(client->ws_conn, message);
        }
        client = client->next;
    }
}

// === WebSocket Callbacks ===

static void on_ws_open(cwh_ws_conn_t *conn, void *user_data)
{
    printf("[WS] Dashboard client connected\n");
}

static void on_ws_message(cwh_ws_conn_t *conn, const cwh_ws_message_t *msg, void *user_data)
{
    if (msg->opcode == CWH_WS_OP_TEXT)
    {
        char *data = (char *)malloc(msg->len + 1);
        memcpy(data, msg->data, msg->len);
        data[msg->len] = '\0';
        printf("[WS] Message from client: %s\n", data);
        free(data);
    }
}

static void on_ws_close(cwh_ws_conn_t *conn, uint16_t code, const char *reason, void *user_data)
{
    dashboard_client_t *client = (dashboard_client_t *)user_data;

    printf("[WS] Dashboard client disconnected: %d - %s\n", code, reason);

    // Remove from client list
    dashboard_client_t **current = &clients;
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

// === HTML Dashboard Client ===

static const char *html_dashboard =
    "<!DOCTYPE html>\n"
    "<html>\n"
    "<head>\n"
    "    <title>Real-Time Dashboard</title>\n"
    "    <style>\n"
    "        body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background: #1a1a1a; color: #fff; }\n"
    "        h1 { text-align: center; }\n"
    "        .container { max-width: 1200px; margin: 0 auto; }\n"
    "        .metrics { display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr)); gap: 20px; margin-bottom: 20px; }\n"
    "        .metric { background: #2a2a2a; padding: 20px; border-radius: 10px; box-shadow: 0 2px 5px rgba(0,0,0,0.3); }\n"
    "        .metric-value { font-size: 2.5em; font-weight: bold; margin: 10px 0; }\n"
    "        .metric-label { color: #888; font-size: 0.9em; }\n"
    "        .chart { background: #2a2a2a; padding: 20px; border-radius: 10px; height: 300px; }\n"
    "        canvas { width: 100% !important; height: 100% !important; }\n"
    "        .status { position: fixed; top: 10px; right: 10px; padding: 10px; border-radius: 5px; }\n"
    "        .status.connected { background: #4CAF50; }\n"
    "        .status.disconnected { background: #f44336; }\n"
    "    </style>\n"
    "</head>\n"
    "<body>\n"
    "    <div class=\"status\" id=\"status\">Connecting...</div>\n"
    "    <div class=\"container\">\n"
    "        <h1>📊 Real-Time System Dashboard</h1>\n"
    "        <div class=\"metrics\">\n"
    "            <div class=\"metric\">\n"
    "                <div class=\"metric-label\">CPU Usage</div>\n"
    "                <div class=\"metric-value\" id=\"cpu\">--</div>\n"
    "            </div>\n"
    "            <div class=\"metric\">\n"
    "                <div class=\"metric-label\">Memory Usage</div>\n"
    "                <div class=\"metric-value\" id=\"memory\">--</div>\n"
    "            </div>\n"
    "            <div class=\"metric\">\n"
    "                <div class=\"metric-label\">Active Connections</div>\n"
    "                <div class=\"metric-value\" id=\"connections\">--</div>\n"
    "            </div>\n"
    "            <div class=\"metric\">\n"
    "                <div class=\"metric-label\">Server Uptime</div>\n"
    "                <div class=\"metric-value\" id=\"uptime\">--</div>\n"
    "            </div>\n"
    "        </div>\n"
    "        <div class=\"chart\">\n"
    "            <canvas id=\"cpuChart\"></canvas>\n"
    "        </div>\n"
    "    </div>\n"
    "    <script>\n"
    "        const ws = new WebSocket('ws://localhost:8081/ws');\n"
    "        const status = document.getElementById('status');\n"
    "        const cpuCanvas = document.getElementById('cpuChart');\n"
    "        const ctx = cpuCanvas.getContext('2d');\n"
    "        \n"
    "        let cpuHistory = [];\n"
    "        const maxHistory = 60;\n"
    "        \n"
    "        ws.onopen = () => {\n"
    "            status.textContent = 'Connected';\n"
    "            status.className = 'status connected';\n"
    "        };\n"
    "        \n"
    "        ws.onclose = () => {\n"
    "            status.textContent = 'Disconnected';\n"
    "            status.className = 'status disconnected';\n"
    "        };\n"
    "        \n"
    "        ws.onmessage = (event) => {\n"
    "            const data = JSON.parse(event.data);\n"
    "            if (data.type === 'metrics') {\n"
    "                document.getElementById('cpu').textContent = data.cpu.toFixed(1) + '%';\n"
    "                document.getElementById('memory').textContent = \n"
    "                    data.memory_used.toFixed(0) + ' / ' + data.memory_total.toFixed(0) + ' MB';\n"
    "                document.getElementById('connections').textContent = data.connections;\n"
    "                document.getElementById('uptime').textContent = formatUptime(data.uptime);\n"
    "                \n"
    "                cpuHistory.push(data.cpu);\n"
    "                if (cpuHistory.length > maxHistory) cpuHistory.shift();\n"
    "                drawChart();\n"
    "            }\n"
    "        };\n"
    "        \n"
    "        function formatUptime(seconds) {\n"
    "            const days = Math.floor(seconds / 86400);\n"
    "            const hours = Math.floor((seconds % 86400) / 3600);\n"
    "            const mins = Math.floor((seconds % 3600) / 60);\n"
    "            return `${days}d ${hours}h ${mins}m`;\n"
    "        }\n"
    "        \n"
    "        function drawChart() {\n"
    "            const width = cpuCanvas.width;\n"
    "            const height = cpuCanvas.height;\n"
    "            ctx.clearRect(0, 0, width, height);\n"
    "            \n"
    "            // Grid\n"
    "            ctx.strokeStyle = '#444';\n"
    "            ctx.lineWidth = 1;\n"
    "            for (let i = 0; i <= 4; i++) {\n"
    "                const y = (height / 4) * i;\n"
    "                ctx.beginPath();\n"
    "                ctx.moveTo(0, y);\n"
    "                ctx.lineTo(width, y);\n"
    "                ctx.stroke();\n"
    "            }\n"
    "            \n"
    "            // CPU line\n"
    "            if (cpuHistory.length > 1) {\n"
    "                ctx.strokeStyle = '#4CAF50';\n"
    "                ctx.lineWidth = 2;\n"
    "                ctx.beginPath();\n"
    "                cpuHistory.forEach((cpu, i) => {\n"
    "                    const x = (width / maxHistory) * i;\n"
    "                    const y = height - (height * cpu / 100);\n"
    "                    if (i === 0) ctx.moveTo(x, y);\n"
    "                    else ctx.lineTo(x, y);\n"
    "                });\n"
    "                ctx.stroke();\n"
    "            }\n"
    "            \n"
    "            // Labels\n"
    "            ctx.fillStyle = '#888';\n"
    "            ctx.font = '12px Arial';\n"
    "            ctx.fillText('100%', 5, 15);\n"
    "            ctx.fillText('0%', 5, height - 5);\n"
    "        }\n"
    "        \n"
    "        // Set canvas size\n"
    "        function resizeCanvas() {\n"
    "            cpuCanvas.width = cpuCanvas.offsetWidth;\n"
    "            cpuCanvas.height = cpuCanvas.offsetHeight;\n"
    "            drawChart();\n"
    "        }\n"
    "        window.addEventListener('resize', resizeCanvas);\n"
    "        resizeCanvas();\n"
    "    </script>\n"
    "</body>\n"
    "</html>";

// === HTTP Request Handler ===

static void handle_http_request(int client_fd, const char *request)
{
    if (strstr(request, "GET / "))
    {
        char response[16384];
        snprintf(response, sizeof(response),
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: text/html\r\n"
                 "Content-Length: %zu\r\n"
                 "\r\n"
                 "%s",
                 strlen(html_dashboard), html_dashboard);
        send(client_fd, response, strlen(response), 0);
#ifdef _WIN32
        closesocket(client_fd);
#else
        close(client_fd);
#endif
    }
    else if (strstr(request, "GET /ws") && cwh_ws_is_upgrade_request(request))
    {
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

                cwh_ws_conn_t *ws_conn = cwh_ws_conn_new(client_fd, false);
                if (ws_conn)
                {
                    dashboard_client_t *client = (dashboard_client_t *)calloc(1, sizeof(dashboard_client_t));
                    client->ws_conn = ws_conn;
                    client->connected_at = time(NULL);
                    client->next = clients;
                    clients = client;
                    client_count++;

                    cwh_ws_callbacks_t callbacks = {
                        .on_open = on_ws_open,
                        .on_message = on_ws_message,
                        .on_close = on_ws_close,
                        .on_error = on_ws_error,
                        .user_data = client};

                    while (ws_conn->state == CWH_WS_STATE_OPEN)
                    {
                        if (cwh_ws_process(ws_conn, &callbacks) < 0)
                            break;
                    }
                }
            }
        }
    }
}

// === Main ===

int main()
{
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    server_start_time = time(NULL);
    srand((unsigned int)server_start_time);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(server_fd, 10);

    printf("=== Real-Time Dashboard Server ===\n");
    printf("Listening on http://localhost:%d\n", PORT);
    printf("Open your browser to see live metrics!\n\n");

    while (1)
    {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

        if (client_fd < 0)
            continue;

        char buffer[4096];
        int received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (received > 0)
        {
            buffer[received] = '\0';
            handle_http_request(client_fd, buffer);
        }

        // Broadcast metrics periodically (in real app, use separate thread)
        static time_t last_broadcast = 0;
        if (time(NULL) - last_broadcast >= 1)
        {
            broadcast_metrics();
            last_broadcast = time(NULL);
        }
    }

    return 0;
}
