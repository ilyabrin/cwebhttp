#ifndef CWEBHTTP_WS_H
#define CWEBHTTP_WS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// WebSocket opcodes (RFC 6455)
typedef enum
{
    CWH_WS_OP_CONTINUATION = 0x0,
    CWH_WS_OP_TEXT = 0x1,
    CWH_WS_OP_BINARY = 0x2,
    CWH_WS_OP_CLOSE = 0x8,
    CWH_WS_OP_PING = 0x9,
    CWH_WS_OP_PONG = 0xA
} cwh_ws_opcode_t;

// WebSocket close codes (RFC 6455)
typedef enum
{
    CWH_WS_CLOSE_NORMAL = 1000,
    CWH_WS_CLOSE_GOING_AWAY = 1001,
    CWH_WS_CLOSE_PROTOCOL_ERROR = 1002,
    CWH_WS_CLOSE_UNSUPPORTED = 1003,
    CWH_WS_CLOSE_NO_STATUS = 1005,
    CWH_WS_CLOSE_ABNORMAL = 1006,
    CWH_WS_CLOSE_INVALID_DATA = 1007,
    CWH_WS_CLOSE_POLICY_VIOLATION = 1008,
    CWH_WS_CLOSE_TOO_LARGE = 1009,
    CWH_WS_CLOSE_EXTENSION_REQUIRED = 1010,
    CWH_WS_CLOSE_UNEXPECTED = 1011
} cwh_ws_close_code_t;

// WebSocket frame header
typedef struct
{
    bool fin;               // Final fragment flag
    uint8_t opcode;         // Opcode (text, binary, close, ping, pong)
    bool mask;              // Mask flag (client->server must be masked)
    uint64_t payload_len;   // Payload length
    uint8_t masking_key[4]; // Masking key (if masked)
} cwh_ws_frame_header_t;

// WebSocket message (after frame assembly)
typedef struct
{
    uint8_t opcode; // Message type
    uint8_t *data;  // Payload data
    size_t len;     // Payload length
} cwh_ws_message_t;

// WebSocket connection state
typedef enum
{
    CWH_WS_STATE_CONNECTING,
    CWH_WS_STATE_OPEN,
    CWH_WS_STATE_CLOSING,
    CWH_WS_STATE_CLOSED
} cwh_ws_state_t;

// WebSocket connection
typedef struct
{
    int fd;                     // Socket file descriptor
    cwh_ws_state_t state;       // Connection state
    bool is_client;             // Client or server side
    uint8_t *recv_buffer;       // Receive buffer
    size_t recv_buffer_size;    // Buffer size
    size_t recv_buffer_len;     // Current data in buffer
    uint8_t *fragment_buffer;   // Fragment reassembly buffer
    size_t fragment_buffer_len; // Current fragment data length
    uint8_t fragment_opcode;    // First fragment opcode
} cwh_ws_conn_t;

// WebSocket event callbacks
typedef void (*cwh_ws_on_open_t)(cwh_ws_conn_t *conn, void *user_data);
typedef void (*cwh_ws_on_message_t)(cwh_ws_conn_t *conn, const cwh_ws_message_t *msg, void *user_data);
typedef void (*cwh_ws_on_close_t)(cwh_ws_conn_t *conn, uint16_t code, const char *reason, void *user_data);
typedef void (*cwh_ws_on_error_t)(cwh_ws_conn_t *conn, const char *error, void *user_data);

// WebSocket callbacks structure
typedef struct
{
    cwh_ws_on_open_t on_open;
    cwh_ws_on_message_t on_message;
    cwh_ws_on_close_t on_close;
    cwh_ws_on_error_t on_error;
    void *user_data;
} cwh_ws_callbacks_t;

// === Core API ===

// Initialize WebSocket connection
cwh_ws_conn_t *cwh_ws_conn_new(int fd, bool is_client);

// Free WebSocket connection
void cwh_ws_conn_free(cwh_ws_conn_t *conn);

// Process incoming data (call this when socket is readable)
int cwh_ws_process(cwh_ws_conn_t *conn, cwh_ws_callbacks_t *callbacks);

// Send text message
int cwh_ws_send_text(cwh_ws_conn_t *conn, const char *text);

// Send binary message
int cwh_ws_send_binary(cwh_ws_conn_t *conn, const uint8_t *data, size_t len);

// Send ping
int cwh_ws_send_ping(cwh_ws_conn_t *conn, const uint8_t *data, size_t len);

// Send pong
int cwh_ws_send_pong(cwh_ws_conn_t *conn, const uint8_t *data, size_t len);

// Send close frame
int cwh_ws_send_close(cwh_ws_conn_t *conn, uint16_t code, const char *reason);

// === WebSocket Handshake (Client) ===

// Generate WebSocket handshake request (client side)
// Returns allocated string with handshake request, must be freed by caller
char *cwh_ws_client_handshake(const char *host, const char *path, const char *origin);

// Validate WebSocket handshake response (client side)
bool cwh_ws_client_validate_handshake(const char *response, const char *expected_accept_key);

// === WebSocket Handshake (Server) ===

// Check if HTTP request is WebSocket upgrade request
bool cwh_ws_is_upgrade_request(const char *headers);

// Generate WebSocket handshake response (server side)
// Returns allocated string with handshake response, must be freed by caller
char *cwh_ws_server_handshake(const char *sec_websocket_key);

// === Frame Encoding/Decoding (Low-level) ===

// Parse WebSocket frame header
int cwh_ws_parse_frame_header(const uint8_t *data, size_t len, cwh_ws_frame_header_t *header);

// Encode WebSocket frame
int cwh_ws_encode_frame(uint8_t *out, size_t out_size, bool fin, uint8_t opcode,
                        const uint8_t *payload, uint64_t payload_len, bool mask);

// Decode WebSocket frame payload (unmask if needed)
void cwh_ws_decode_payload(uint8_t *data, uint64_t len, const uint8_t masking_key[4]);

// === Utilities ===

// Generate WebSocket key (base64 encoded random 16 bytes)
void cwh_ws_generate_key(char *key_out, size_t key_size);

// Calculate WebSocket accept key from client key
void cwh_ws_calculate_accept_key(const char *client_key, char *accept_key_out, size_t accept_key_size);

// Get opcode name string
const char *cwh_ws_opcode_str(uint8_t opcode);

// Get close code string
const char *cwh_ws_close_code_str(uint16_t code);

#endif // CWEBHTTP_WS_H
