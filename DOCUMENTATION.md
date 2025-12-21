# cwebhttp - Complete Documentation

**Version:** 0.9.0 | **Updated:** December 20, 2025

Modern, lightweight HTTP/WebSocket library in pure C. Zero-copy parser, async I/O, TLS support.

**Quick Links:** [Installation](#installation) • [HTTP Client](#http-client) • [HTTP Server](#http-server) • [WebSocket](#websocket) • [HTTPS/TLS](#httpstls) • [IoT/Embedded](#iot--embedded) • [API](#api-reference)

---

## Quick Start

### HTTP Client (3 lines)

```c
#include "cwebhttp.h"
char *html = cwh_get("http://example.com");
printf("%s\n", html);
free(html);
```

**Build:** `gcc app.c cwebhttp/src/cwebhttp.c -Icwebhttp/include -lz`

### HTTP Server (15 lines)

```c
#include "cwebhttp_async.h"

void handle(cwh_async_conn_t *conn, cwh_request_t *req, void *data) {
    cwh_async_send_response(conn, 200, "text/html", "<h1>Hello!</h1>", 15);
}

int main() {
    cwh_async_server_t *srv = cwh_async_server_new();
    cwh_async_server_route(srv, "GET", "/", handle, NULL);
    cwh_async_server_listen(srv, 8080);
    cwh_async_server_run(srv);
}
```

**Build:** `gcc server.c cwebhttp/src/cwebhttp.c cwebhttp/src/async/*.c -Iinclude -lz`

### WebSocket (Browser + Server)

**Browser:**

```javascript
const ws = new WebSocket('ws://localhost:8080/ws');
ws.onmessage = (e) => console.log(e.data);
ws.send('Hello!');
```

**Server:**

```c
void on_message(cwh_ws_conn_t *ws, const cwh_ws_message_t *msg, void *data) {
    cwh_ws_send_text(ws, (const char *)msg->data);  // Echo
}

// In HTTP server:
if (cwh_ws_is_upgrade_request(buffer)) {
    char *handshake = cwh_ws_server_handshake(key);
    send(fd, handshake, strlen(handshake), 0);
    
    cwh_ws_conn_t *ws = cwh_ws_conn_new(fd, false);
    cwh_ws_callbacks_t cb = {.on_message = on_message};
    while (ws->state == CWH_WS_STATE_OPEN)
        cwh_ws_process(ws, &cb);
}
```

**Build:** `gcc ws.c cwebhttp/src/cwebhttp.c cwebhttp/src/websocket.c -Iinclude -lz`

---

## Installation

### Quick Setup

```bash
# Clone
git clone https://github.com/ilyabrin/cwebhttp.git

# Or as submodule
git submodule add https://github.com/ilyabrin/cwebhttp.git deps/cwebhttp

# Test
cd cwebhttp && make test
```

### Prerequisites

- **Required:** C compiler, zlib (`-lz`)
- **Optional:** mbedTLS (for HTTPS)
  - Windows: `pacman -S mingw-w64-x86_64-mbedtls`
  - Linux: `sudo apt-get install libmbedtls-dev`
  - macOS: `brew install mbedtls`

---

## Integration

### Files Needed

| Feature      | Files             | Libs        | Size     |
| ------------ | ----------------- | ----------- | -------- |
| HTTP Client  | `cwebhttp.c`      | `-lz`       | 68KB     |
| HTTP Server  | `cwebhttp.c`      | `-lz`       | 68KB     |
| Async Server | + `async/*.c`     | `-lz`       | 85KB     |
| WebSocket    | + `websocket.c`   | `-lz`       | 155KB    |
| HTTPS/TLS    | + `tls_mbedtls.c` | `-lmbedtls` | +mbedTLS |

### Makefile

```makefile
CFLAGS = -Icwebhttp/include
SRCS = app.c cwebhttp/src/cwebhttp.c
LIBS = -lz

ifeq ($(OS),Windows_NT)
    LIBS += -lws2_32
endif

ifdef ENABLE_WS
    SRCS += cwebhttp/src/websocket.c
endif

ifdef ENABLE_TLS
    SRCS += cwebhttp/src/tls_mbedtls.c
    LIBS += -lmbedtls -lmbedx509 -lmbedcrypto
endif

app: $(SRCS)
 $(CC) $(CFLAGS) $^ -o $@ $(LIBS)
```

### CMake

```cmake
add_subdirectory(deps/cwebhttp)
add_executable(app main.c)
target_link_libraries(app PRIVATE cwebhttp)
```

---

## HTTP Client

### Basic Requests

```c
// GET
char *resp = cwh_get("http://api.example.com/users");
free(resp);

// POST
const char *json = "{\"name\":\"John\"}";
char *resp = cwh_post("http://api.example.com/users", "application/json", json);
free(resp);

// PUT, DELETE
char *resp = cwh_put(url, type, body);
char *resp = cwh_delete(url);
```

### Custom Headers

```c
cwh_request_t req = {0};
req.method_str = "GET";
req.path = "http://api.example.com/data";
req.headers[0] = "Authorization";
req.headers[1] = "Bearer TOKEN";
req.num_headers = 1;

char *resp = cwh_send_request(&req);
free(resp);
```

### Error Handling

```c
char *resp = cwh_get(url);
if (!resp) {
    cwh_error_t *err = cwh_get_last_error();
    printf("Error: %s\n", cwh_error_message(err));
}
```

### REST API Pattern

```c
typedef struct {
    char *base_url, *token;
} api_t;

char* api_get(api_t *api, const char *endpoint) {
    char url[512];
    snprintf(url, sizeof(url), "%s%s", api->base_url, endpoint);
    cwh_request_t req = {.method_str = "GET", .path = url};
    req.headers[0] = "Authorization";
    req.headers[1] = api->token;
    req.num_headers = 1;
    return cwh_send_request(&req);
}
```

---

## HTTP Server

### Async Server

```c
#include "cwebhttp_async.h"

void handle_home(cwh_async_conn_t *conn, cwh_request_t *req, void *data) {
    cwh_async_send_response(conn, 200, "text/html", "<h1>Home</h1>", 12);
}

void handle_api(cwh_async_conn_t *conn, cwh_request_t *req, void *data) {
    cwh_async_send_json(conn, 200, "{\"status\":\"ok\"}");
}

void handle_404(cwh_async_conn_t *conn, cwh_request_t *req, void *data) {
    cwh_async_send_status(conn, 404, "Not Found");
}

int main() {
    cwh_async_server_t *srv = cwh_async_server_new();
    cwh_async_server_route(srv, "GET", "/", handle_home, NULL);
    cwh_async_server_route(srv, "GET", "/api/*", handle_api, NULL);
    cwh_async_server_set_404_handler(srv, handle_404, NULL);
    
    cwh_async_server_listen(srv, 8080);
    cwh_async_server_run(srv);  // Uses epoll/kqueue/IOCP automatically
    
    cwh_async_server_free(srv);
}
```

### Static Files

```c
void handle_static(cwh_async_conn_t *conn, cwh_request_t *req, void *data) {
    if (strstr(req->path, "..")) {  // Security check
        cwh_async_send_status(conn, 403, "Forbidden");
        return;
    }
    
    char path[512];
    snprintf(path, sizeof(path), "./www%s", req->path);
    cwh_async_send_file(conn, path);
}
```

---

## WebSocket

### Multi-Client Chat Server

```c
#define MAX_CLIENTS 10
cwh_ws_conn_t *clients[MAX_CLIENTS];

void broadcast(const char *msg) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && clients[i]->state == CWH_WS_STATE_OPEN) {
            cwh_ws_send_text(clients[i], msg);
        }
    }
}

void on_message(cwh_ws_conn_t *ws, const cwh_ws_message_t *msg, void *data) {
    if (msg->opcode == CWH_WS_OP_TEXT) {
        printf("RX: %.*s\n", (int)msg->len, msg->data);
        broadcast((const char *)msg->data);  // Broadcast to all
    }
}

// In HTTP server loop:
if (cwh_ws_is_upgrade_request(buffer)) {
    // Extract key, send handshake
    char key[64];
    // ... extract from buffer ...
    char *handshake = cwh_ws_server_handshake(key);
    send(fd, handshake, strlen(handshake), 0);
    free(handshake);
    
    // Create WebSocket
    cwh_ws_conn_t *ws = cwh_ws_conn_new(fd, false);
    clients[find_free_slot()] = ws;
    
    cwh_ws_callbacks_t cb = {.on_message = on_message};
    while (ws->state == CWH_WS_STATE_OPEN) {
        cwh_ws_process(ws, &cb);
    }
    
    cwh_ws_conn_free(ws);
}
```

### Browser Client

```html
<script>
const ws = new WebSocket('ws://localhost:8080/ws');

ws.onopen = () => ws.send(JSON.stringify({type:'join', user:'Alice'}));

ws.onmessage = (e) => {
    const msg = JSON.parse(e.data);
    console.log(`${msg.user}: ${msg.text}`);
};

function send(text) {
    ws.send(JSON.stringify({type:'message', text: text}));
}
</script>
```

### Power-Efficient Loop (IoT)

```c
fd_set fds;
struct timeval tv = {5, 0};

while (ws->state == CWH_WS_STATE_OPEN) {
    FD_ZERO(&fds);
    FD_SET(ws->fd, &fds);
    
    if (select(ws->fd+1, &fds, NULL, NULL, &tv) > 0) {
        cwh_ws_process(ws, &cb);
    } else {
        // Timeout - send ping or sleep
        cwh_ws_send_ping(ws, NULL, 0);
    }
}
```

---

## HTTPS/TLS

### HTTPS Client

```c
#include "cwebhttp_tls.h"

cwh_tls_context_t *tls = cwh_tls_context_new();
cwh_tls_context_set_ca_cert(tls, "ca-cert.pem");  // Optional

char *resp = cwh_get("https://api.github.com");  // Automatic TLS!
free(resp);

cwh_tls_context_free(tls);
```

**Build:** `gcc app.c src/cwebhttp.c src/tls_mbedtls.c -Iinclude -lmbedtls -lmbedx509 -lmbedcrypto -lz`

### HTTPS Server

```c
#include "cwebhttp_tls.h"

cwh_async_server_t *srv = cwh_async_server_new();

// Load certificates
cwh_tls_context_t *tls = cwh_tls_context_new();
cwh_tls_context_set_cert(tls, "server-cert.pem", "server-key.pem");
cwh_async_server_set_tls(srv, tls);

// Now serves HTTPS!
cwh_async_server_listen(srv, 443);
cwh_async_server_run(srv);
```

### Generate Certificate

```bash
openssl genrsa -out key.pem 2048
openssl req -new -x509 -key key.pem -out cert.pem -days 365
```

### WebSocket + TLS (WSS)

```javascript
// Browser: just use wss://
const ws = new WebSocket('wss://yourdomain.com/ws');
```

```c
// Server: TLS already applied to server
cwh_async_server_set_tls(srv, tls);  // WSS now works!
```

---

## IoT & Embedded

### Platform Support

| Platform     | RAM    | HTTP | WS  | TLS | Status    |
| ------------ | ------ | ---- | --- | --- | --------- |
| Raspberry Pi | 512MB+ | ✅    | ✅   | ✅   | Full      |
| ESP32        | 520KB  | ✅    | ✅*  | ✅   | Optimized |
| ESP8266      | 80KB   | ✅    | ⚠️*  | ⚠️   | Limited   |
| STM32F4      | 192KB  | ✅    | ✅*  | ✅   | Optimized |

*Requires buffer optimization

### ESP32 Optimization

**Edit `src/websocket.c` lines 23-24:**

```c
#define WS_DEFAULT_RECV_BUFFER_SIZE (4 * 1024)   // 4KB (was 64KB)
#define WS_MAX_FRAGMENT_SIZE (16 * 1024)         // 16KB (was 10MB)
```

**Saves 60KB RAM per connection!**

**Limit connections:**

```c
#define MAX_CLIENTS 4  // ESP32 can handle 4-6
```

**ESP32 Example:**

```c
#include <freertos/FreeRTOS.h>
#include "cwebhttp_ws.h"

#define MAX_CLIENTS 4
cwh_ws_conn_t *clients[MAX_CLIENTS];

void broadcast_sensor(float temp) {
    char msg[128];
    snprintf(msg, sizeof(msg), "{\"temp\":%.1f}", temp);
    for (int i = 0; i < MAX_CLIENTS; i++)
        if (clients[i] && clients[i]->state == CWH_WS_STATE_OPEN)
            cwh_ws_send_text(clients[i], msg);
}

void sensor_task(void *p) {
    while (1) {
        broadcast_sensor(read_temperature());
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}
```

### STM32 Static Allocation

```c
#define MAX_CONNS 2
static uint8_t ws_buffers[MAX_CONNS][4096];
static cwh_ws_conn_t ws_pool[MAX_CONNS];
static bool ws_used[MAX_CONNS];

cwh_ws_conn_t* ws_acquire(int fd) {
    for (int i = 0; i < MAX_CONNS; i++) {
        if (!ws_used[i]) {
            ws_used[i] = true;
            ws_pool[i].fd = fd;
            ws_pool[i].recv_buffer = ws_buffers[i];
            return &ws_pool[i];
        }
    }
    return NULL;
}
```

---

## API Reference

### HTTP Client

```c
char* cwh_get(const char *url);
char* cwh_post(const char *url, const char *type, const char *body);
char* cwh_put(const char *url, const char *type, const char *body);
char* cwh_delete(const char *url);
char* cwh_send_request(cwh_request_t *req);
```

### HTTP Server methods

```c
cwh_async_server_t* cwh_async_server_new();
void cwh_async_server_free(cwh_async_server_t *srv);
void cwh_async_server_listen(cwh_async_server_t *srv, int port);
void cwh_async_server_run(cwh_async_server_t *srv);
void cwh_async_server_route(cwh_async_server_t *srv, const char *method,
                            const char *path, cwh_async_handler_t handler, void *data);
void cwh_async_send_response(cwh_async_conn_t *conn, int status,
                             const char *type, const char *body, size_t len);
void cwh_async_send_json(cwh_async_conn_t *conn, int status, const char *json);
```

### WebSocket methods

```c
cwh_ws_conn_t* cwh_ws_conn_new(int fd, bool is_client);
void cwh_ws_conn_free(cwh_ws_conn_t *conn);
int cwh_ws_process(cwh_ws_conn_t *conn, cwh_ws_callbacks_t *cb);
int cwh_ws_send_text(cwh_ws_conn_t *conn, const char *text);
int cwh_ws_send_binary(cwh_ws_conn_t *conn, const uint8_t *data, size_t len);
int cwh_ws_send_ping(cwh_ws_conn_t *conn, const uint8_t *data, size_t len);
int cwh_ws_send_close(cwh_ws_conn_t *conn, uint16_t code, const char *reason);
char* cwh_ws_server_handshake(const char *key);
bool cwh_ws_is_upgrade_request(const char *headers);
```

### TLS

```c
cwh_tls_context_t* cwh_tls_context_new();
void cwh_tls_context_free(cwh_tls_context_t *ctx);
void cwh_tls_context_set_cert(cwh_tls_context_t *ctx, const char *cert, const char *key);
void cwh_tls_context_set_ca_cert(cwh_tls_context_t *ctx, const char *ca);
void cwh_async_server_set_tls(cwh_async_server_t *srv, cwh_tls_context_t *tls);
```

### Error Handling

```c
cwh_error_t* cwh_get_last_error();
int cwh_error_code(const cwh_error_t *err);
const char* cwh_error_message(const cwh_error_t *err);
```

---

## Performance

| Metric                        | Value                             |
| ----------------------------- | --------------------------------- |
| Parser speed                  | 2.5 GB/s                          |
| Binary size (HTTP)            | 68 KB                             |
| Binary size (WebSocket)       | 155 KB                            |
| Memory per connection         | <1 KB (HTTP), 6 KB (WS optimized) |
| Allocations per parse         | 0 (zero-copy)                     |
| Max connections (Linux/epoll) | 100,000+                          |
| Throughput (async server)     | 50-60K req/s                      |

---

## Troubleshooting

### Build Issues

- **Undefined reference**: Link all .c files
- **Cannot find headers**: Add `-Icwebhttp/include`
- **Windows errors**: Add `-lws2_32`
- **TLS errors**: Install mbedTLS, add `-lmbedtls -lmbedx509 -lmbedcrypto`

### Runtime Issues

- **Connection refused**: Check server running, port not blocked
- **WebSocket handshake failed**: Check HTTP headers include `Upgrade: websocket`
- **Memory leak**: Always `free()` returned strings
- **Slow**: Use async I/O, enable keep-alive

### Common Mistakes

```c
// ❌ Wrong - memory leak
char *r = cwh_get(url);

// ✅ Correct
char *r = cwh_get(url);
if (r) { printf("%s\n", r); free(r); }

// ❌ Wrong - busy loop
while (ws->state == CWH_WS_STATE_OPEN)
    cwh_ws_process(ws, &cb);

// ✅ Correct - use select()
fd_set fds; struct timeval tv = {1,0};
while (ws->state == CWH_WS_STATE_OPEN) {
    FD_ZERO(&fds); FD_SET(ws->fd, &fds);
    if (select(ws->fd+1, &fds, NULL, NULL, &tv) > 0)
        cwh_ws_process(ws, &cb);
}
```

---

## Examples

All in `examples/` directory:

| File               | Description                     | Build                                                                        |
| ------------------ | ------------------------------- | ---------------------------------------------------------------------------- |
| `simple_client.c`  | HTTP GET/POST                   | `gcc examples/simple_client.c src/cwebhttp.c -Iinclude -lz`                  |
| `async_server.c`   | Production server               | `gcc examples/async_server.c src/cwebhttp.c src/async/*.c -Iinclude -lz`     |
| `ws_chat_server.c` | WebSocket chat (422 lines)      | `gcc examples/ws_chat_server.c src/cwebhttp.c src/websocket.c -Iinclude -lz` |
| `ws_dashboard.c`   | Real-time dashboard (458 lines) | `gcc examples/ws_dashboard.c src/cwebhttp.c src/websocket.c -Iinclude -lz`   |

**Run chat server:**

```bash
./build/examples/ws_chat_server
# Open http://localhost:8080 in browser
```

**Run dashboard:**

```bash
./build/examples/ws_dashboard
# Open http://localhost:8081 in browser
```

---

## Memory Management

### Leak Detection

```c
#define ENABLE_MEMCHECK 1
#include "memcheck.h"

memcheck_init();
// ... your code ...
memcheck_report();  // Shows leaks
memcheck_cleanup();
```

### Best Practices

- Always `free()` returned strings
- Call `*_free()` functions on cleanup
- Use `select()` instead of busy-waiting
- Limit connections on IoT devices

---

## Support

- **Docs**: README.md, CHANGELOG.md
- **Examples**: `examples/` directory
- **GitHub**: <https://github.com/ilyabrin/cwebhttp>
- **Issues**: GitHub Issues

---

**License:** MIT  
**Version:** 0.9.0  
**Status:** Production Ready ✅
