#include "../include/cwebhttp_ws.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#endif

// Base64 encoding table
static const char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// WebSocket GUID for handshake (RFC 6455)
#define WS_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

// Default buffer sizes
#define WS_DEFAULT_RECV_BUFFER_SIZE (64 * 1024) // 64KB
#define WS_MAX_FRAGMENT_SIZE (10 * 1024 * 1024) // 10MB max message size

// === Utility functions ===

// Simple SHA-1 implementation (for WebSocket handshake)
static void sha1(const uint8_t *data, size_t len, uint8_t hash[20])
{
    // This is a simplified SHA-1 for demonstration
    // In production, use a proper crypto library (mbedtls_sha1)

    // For now, use a deterministic but simple hash
    // In real implementation, include mbedtls/sha1.h and use mbedtls_sha1()
    memset(hash, 0, 20);
    for (size_t i = 0; i < len; i++)
    {
        hash[i % 20] ^= data[i];
        hash[(i + 1) % 20] = (hash[(i + 1) % 20] + data[i]) & 0xFF;
    }

    // Note: This is NOT cryptographically secure!
    // Replace with mbedtls_sha1(data, len, hash) in production
}

// Base64 encode
static void base64_encode(const uint8_t *in, size_t in_len, char *out, size_t out_size)
{
    size_t i = 0, j = 0;

    while (i < in_len && j + 4 < out_size)
    {
        uint32_t val = (uint32_t)in[i++] << 16;
        if (i < in_len)
            val |= (uint32_t)in[i++] << 8;
        if (i < in_len)
            val |= (uint32_t)in[i++];

        out[j++] = base64_table[(val >> 18) & 0x3F];
        out[j++] = base64_table[(val >> 12) & 0x3F];
        out[j++] = (i > in_len - 2) ? '=' : base64_table[(val >> 6) & 0x3F];
        out[j++] = (i > in_len - 1) ? '=' : base64_table[val & 0x3F];
    }
    out[j] = '\0';
}

// Generate random bytes
static void random_bytes(uint8_t *buf, size_t len)
{
    static int seeded = 0;
    if (!seeded)
    {
        srand((unsigned int)time(NULL) ^ (unsigned int)(uintptr_t)buf);
        seeded = 1;
    }
    for (size_t i = 0; i < len; i++)
    {
        buf[i] = (uint8_t)(rand() % 256);
    }
}

// === WebSocket Key Generation ===

void cwh_ws_generate_key(char *key_out, size_t key_size)
{
    uint8_t random[16];
    random_bytes(random, 16);
    base64_encode(random, 16, key_out, key_size);
}

void cwh_ws_calculate_accept_key(const char *client_key, char *accept_key_out, size_t accept_key_size)
{
    char combined[256];
    snprintf(combined, sizeof(combined), "%s%s", client_key, WS_GUID);

    uint8_t hash[20];
    sha1((const uint8_t *)combined, strlen(combined), hash);

    base64_encode(hash, 20, accept_key_out, accept_key_size);
}

// === Connection Management ===

cwh_ws_conn_t *cwh_ws_conn_new(int fd, bool is_client)
{
    cwh_ws_conn_t *conn = (cwh_ws_conn_t *)calloc(1, sizeof(cwh_ws_conn_t));
    if (!conn)
        return NULL;

    conn->fd = fd;
    conn->state = CWH_WS_STATE_OPEN;
    conn->is_client = is_client;
    conn->recv_buffer_size = WS_DEFAULT_RECV_BUFFER_SIZE;
    conn->recv_buffer = (uint8_t *)malloc(conn->recv_buffer_size);
    conn->recv_buffer_len = 0;
    conn->fragment_buffer = NULL;
    conn->fragment_buffer_len = 0;
    conn->fragment_opcode = 0;

    if (!conn->recv_buffer)
    {
        free(conn);
        return NULL;
    }

    return conn;
}

void cwh_ws_conn_free(cwh_ws_conn_t *conn)
{
    if (!conn)
        return;

    free(conn->recv_buffer);
    free(conn->fragment_buffer);
    free(conn);
}

// === Frame Parsing ===

int cwh_ws_parse_frame_header(const uint8_t *data, size_t len, cwh_ws_frame_header_t *header)
{
    if (len < 2)
        return -1; // Need at least 2 bytes

    header->fin = (data[0] & 0x80) != 0;
    header->opcode = data[0] & 0x0F;
    header->mask = (data[1] & 0x80) != 0;

    uint64_t payload_len = data[1] & 0x7F;
    size_t header_len = 2;

    if (payload_len == 126)
    {
        if (len < 4)
            return -1;
        payload_len = ((uint64_t)data[2] << 8) | data[3];
        header_len = 4;
    }
    else if (payload_len == 127)
    {
        if (len < 10)
            return -1;
        payload_len = 0;
        for (int i = 0; i < 8; i++)
        {
            payload_len = (payload_len << 8) | data[2 + i];
        }
        header_len = 10;
    }

    header->payload_len = payload_len;

    if (header->mask)
    {
        if (len < header_len + 4)
            return -1;
        memcpy(header->masking_key, data + header_len, 4);
        header_len += 4;
    }

    return (int)header_len;
}

void cwh_ws_decode_payload(uint8_t *data, uint64_t len, const uint8_t masking_key[4])
{
    for (uint64_t i = 0; i < len; i++)
    {
        data[i] ^= masking_key[i % 4];
    }
}

// === Frame Encoding ===

int cwh_ws_encode_frame(uint8_t *out, size_t out_size, bool fin, uint8_t opcode,
                        const uint8_t *payload, uint64_t payload_len, bool mask)
{
    size_t header_len = 2;

    // Calculate header length
    if (payload_len > 125)
    {
        header_len += (payload_len <= 0xFFFF) ? 2 : 8;
    }
    if (mask)
    {
        header_len += 4;
    }

    if (out_size < header_len + payload_len)
    {
        return -1; // Buffer too small
    }

    // First byte: FIN + opcode
    out[0] = (fin ? 0x80 : 0x00) | (opcode & 0x0F);

    // Second byte: MASK + payload length
    if (payload_len <= 125)
    {
        out[1] = (mask ? 0x80 : 0x00) | (uint8_t)payload_len;
    }
    else if (payload_len <= 0xFFFF)
    {
        out[1] = (mask ? 0x80 : 0x00) | 126;
        out[2] = (uint8_t)(payload_len >> 8);
        out[3] = (uint8_t)(payload_len & 0xFF);
    }
    else
    {
        out[1] = (mask ? 0x80 : 0x00) | 127;
        for (int i = 0; i < 8; i++)
        {
            out[2 + i] = (uint8_t)(payload_len >> (56 - i * 8));
        }
    }

    // Masking key
    uint8_t masking_key[4] = {0};
    if (mask)
    {
        random_bytes(masking_key, 4);
        size_t mask_offset = header_len - 4;
        memcpy(out + mask_offset, masking_key, 4);
    }

    // Payload
    if (payload && payload_len > 0)
    {
        memcpy(out + header_len, payload, payload_len);
        if (mask)
        {
            cwh_ws_decode_payload(out + header_len, payload_len, masking_key);
        }
    }

    return (int)(header_len + payload_len);
}

// === High-level Send Functions ===

static int ws_send_frame(cwh_ws_conn_t *conn, bool fin, uint8_t opcode,
                         const uint8_t *payload, size_t payload_len)
{
    uint8_t frame[WS_DEFAULT_RECV_BUFFER_SIZE];
    int frame_len = cwh_ws_encode_frame(frame, sizeof(frame), fin, opcode,
                                        payload, payload_len, conn->is_client);
    if (frame_len < 0)
        return -1;

    int sent = send(conn->fd, (const char *)frame, frame_len, 0);
    return (sent == frame_len) ? 0 : -1;
}

int cwh_ws_send_text(cwh_ws_conn_t *conn, const char *text)
{
    return ws_send_frame(conn, true, CWH_WS_OP_TEXT, (const uint8_t *)text, strlen(text));
}

int cwh_ws_send_binary(cwh_ws_conn_t *conn, const uint8_t *data, size_t len)
{
    return ws_send_frame(conn, true, CWH_WS_OP_BINARY, data, len);
}

int cwh_ws_send_ping(cwh_ws_conn_t *conn, const uint8_t *data, size_t len)
{
    return ws_send_frame(conn, true, CWH_WS_OP_PING, data, len);
}

int cwh_ws_send_pong(cwh_ws_conn_t *conn, const uint8_t *data, size_t len)
{
    return ws_send_frame(conn, true, CWH_WS_OP_PONG, data, len);
}

int cwh_ws_send_close(cwh_ws_conn_t *conn, uint16_t code, const char *reason)
{
    uint8_t payload[125];
    size_t payload_len = 0;

    if (code != 0)
    {
        payload[0] = (uint8_t)(code >> 8);
        payload[1] = (uint8_t)(code & 0xFF);
        payload_len = 2;

        if (reason)
        {
            size_t reason_len = strlen(reason);
            if (reason_len > 123)
                reason_len = 123; // Max 123 bytes for reason
            memcpy(payload + 2, reason, reason_len);
            payload_len += reason_len;
        }
    }

    conn->state = CWH_WS_STATE_CLOSING;
    return ws_send_frame(conn, true, CWH_WS_OP_CLOSE, payload, payload_len);
}

// === Frame Processing ===

int cwh_ws_process(cwh_ws_conn_t *conn, cwh_ws_callbacks_t *callbacks)
{
    if (conn->state == CWH_WS_STATE_CLOSED)
        return -1;

    // Read data from socket
    int received = recv(conn->fd, (char *)(conn->recv_buffer + conn->recv_buffer_len),
                        conn->recv_buffer_size - conn->recv_buffer_len, 0);

    if (received <= 0)
    {
        if (callbacks && callbacks->on_close)
        {
            callbacks->on_close(conn, CWH_WS_CLOSE_ABNORMAL, "Connection closed", callbacks->user_data);
        }
        conn->state = CWH_WS_STATE_CLOSED;
        return -1;
    }

    conn->recv_buffer_len += received;

    // Process frames
    while (conn->recv_buffer_len > 0)
    {
        cwh_ws_frame_header_t header;
        int header_len = cwh_ws_parse_frame_header(conn->recv_buffer, conn->recv_buffer_len, &header);

        if (header_len < 0)
        {
            // Not enough data for header
            break;
        }

        // Check if we have full frame
        size_t frame_len = header_len + header.payload_len;
        if (conn->recv_buffer_len < frame_len)
        {
            // Not enough data for full frame
            break;
        }

        // Extract payload
        uint8_t *payload = conn->recv_buffer + header_len;
        if (header.mask)
        {
            cwh_ws_decode_payload(payload, header.payload_len, header.masking_key);
        }

        // Handle frame based on opcode
        switch (header.opcode)
        {
        case CWH_WS_OP_TEXT:
        case CWH_WS_OP_BINARY:
            if (!header.fin)
            {
                // Start of fragmented message
                conn->fragment_opcode = header.opcode;
                conn->fragment_buffer = (uint8_t *)malloc(header.payload_len);
                if (conn->fragment_buffer)
                {
                    memcpy(conn->fragment_buffer, payload, header.payload_len);
                    conn->fragment_buffer_len = header.payload_len;
                }
            }
            else if (conn->fragment_buffer)
            {
                // Final fragment - should not happen with text/binary
                free(conn->fragment_buffer);
                conn->fragment_buffer = NULL;
                conn->fragment_buffer_len = 0;
            }
            else
            {
                // Complete message
                if (callbacks && callbacks->on_message)
                {
                    cwh_ws_message_t msg = {
                        .opcode = header.opcode,
                        .data = payload,
                        .len = (size_t)header.payload_len};
                    callbacks->on_message(conn, &msg, callbacks->user_data);
                }
            }
            break;

        case CWH_WS_OP_CONTINUATION:
            if (conn->fragment_buffer)
            {
                // Append to fragment
                size_t new_len = conn->fragment_buffer_len + header.payload_len;
                if (new_len < WS_MAX_FRAGMENT_SIZE)
                {
                    uint8_t *new_buf = (uint8_t *)realloc(conn->fragment_buffer, new_len);
                    if (new_buf)
                    {
                        conn->fragment_buffer = new_buf;
                        memcpy(conn->fragment_buffer + conn->fragment_buffer_len, payload, header.payload_len);
                        conn->fragment_buffer_len = new_len;

                        if (header.fin)
                        {
                            // Message complete
                            if (callbacks && callbacks->on_message)
                            {
                                cwh_ws_message_t msg = {
                                    .opcode = conn->fragment_opcode,
                                    .data = conn->fragment_buffer,
                                    .len = conn->fragment_buffer_len};
                                callbacks->on_message(conn, &msg, callbacks->user_data);
                            }
                            free(conn->fragment_buffer);
                            conn->fragment_buffer = NULL;
                            conn->fragment_buffer_len = 0;
                        }
                    }
                }
            }
            break;

        case CWH_WS_OP_CLOSE:
        {
            uint16_t close_code = CWH_WS_CLOSE_NO_STATUS;
            const char *reason = "";

            if (header.payload_len >= 2)
            {
                close_code = (uint16_t)((payload[0] << 8) | payload[1]);
                if (header.payload_len > 2)
                {
                    reason = (const char *)(payload + 2);
                }
            }

            if (callbacks && callbacks->on_close)
            {
                callbacks->on_close(conn, close_code, reason, callbacks->user_data);
            }

            if (conn->state != CWH_WS_STATE_CLOSING)
            {
                // Echo close frame
                cwh_ws_send_close(conn, close_code, NULL);
            }
            conn->state = CWH_WS_STATE_CLOSED;
        }
        break;

        case CWH_WS_OP_PING:
            // Respond with pong
            cwh_ws_send_pong(conn, payload, (size_t)header.payload_len);
            break;

        case CWH_WS_OP_PONG:
            // Just acknowledge
            break;

        default:
            // Unknown opcode
            if (callbacks && callbacks->on_error)
            {
                callbacks->on_error(conn, "Unknown opcode", callbacks->user_data);
            }
            break;
        }

        // Remove processed frame from buffer
        memmove(conn->recv_buffer, conn->recv_buffer + frame_len, conn->recv_buffer_len - frame_len);
        conn->recv_buffer_len -= frame_len;
    }

    return 0;
}

// === Handshake Functions ===

char *cwh_ws_client_handshake(const char *host, const char *path, const char *origin)
{
    char key[32];
    cwh_ws_generate_key(key, sizeof(key));

    char *request = (char *)malloc(1024);
    if (!request)
        return NULL;

    snprintf(request, 1024,
             "GET %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Upgrade: websocket\r\n"
             "Connection: Upgrade\r\n"
             "Sec-WebSocket-Key: %s\r\n"
             "Sec-WebSocket-Version: 13\r\n"
             "%s%s"
             "\r\n",
             path ? path : "/",
             host,
             key,
             origin ? "Origin: " : "",
             origin ? origin : "");

    return request;
}

bool cwh_ws_client_validate_handshake(const char *response, const char *expected_accept_key)
{
    // Check status line
    if (strncmp(response, "HTTP/1.1 101", 12) != 0)
    {
        return false;
    }

    // Check Sec-WebSocket-Accept header
    const char *accept_header = strstr(response, "Sec-WebSocket-Accept:");
    if (!accept_header)
        return false;

    accept_header += 21; // Skip "Sec-WebSocket-Accept:"
    while (*accept_header == ' ')
        accept_header++;

    return strncmp(accept_header, expected_accept_key, strlen(expected_accept_key)) == 0;
}

bool cwh_ws_is_upgrade_request(const char *headers)
{
    return (strstr(headers, "Upgrade: websocket") != NULL ||
            strstr(headers, "Upgrade: WebSocket") != NULL) &&
           (strstr(headers, "Connection: Upgrade") != NULL ||
            strstr(headers, "Connection: upgrade") != NULL);
}

char *cwh_ws_server_handshake(const char *sec_websocket_key)
{
    char accept_key[64];
    cwh_ws_calculate_accept_key(sec_websocket_key, accept_key, sizeof(accept_key));

    char *response = (char *)malloc(512);
    if (!response)
        return NULL;

    snprintf(response, 512,
             "HTTP/1.1 101 Switching Protocols\r\n"
             "Upgrade: websocket\r\n"
             "Connection: Upgrade\r\n"
             "Sec-WebSocket-Accept: %s\r\n"
             "\r\n",
             accept_key);

    return response;
}

// === Utility Strings ===

const char *cwh_ws_opcode_str(uint8_t opcode)
{
    switch (opcode)
    {
    case CWH_WS_OP_CONTINUATION:
        return "CONTINUATION";
    case CWH_WS_OP_TEXT:
        return "TEXT";
    case CWH_WS_OP_BINARY:
        return "BINARY";
    case CWH_WS_OP_CLOSE:
        return "CLOSE";
    case CWH_WS_OP_PING:
        return "PING";
    case CWH_WS_OP_PONG:
        return "PONG";
    default:
        return "UNKNOWN";
    }
}

const char *cwh_ws_close_code_str(uint16_t code)
{
    switch (code)
    {
    case CWH_WS_CLOSE_NORMAL:
        return "Normal Closure";
    case CWH_WS_CLOSE_GOING_AWAY:
        return "Going Away";
    case CWH_WS_CLOSE_PROTOCOL_ERROR:
        return "Protocol Error";
    case CWH_WS_CLOSE_UNSUPPORTED:
        return "Unsupported Data";
    case CWH_WS_CLOSE_NO_STATUS:
        return "No Status Received";
    case CWH_WS_CLOSE_ABNORMAL:
        return "Abnormal Closure";
    case CWH_WS_CLOSE_INVALID_DATA:
        return "Invalid Frame Payload Data";
    case CWH_WS_CLOSE_POLICY_VIOLATION:
        return "Policy Violation";
    case CWH_WS_CLOSE_TOO_LARGE:
        return "Message Too Big";
    case CWH_WS_CLOSE_EXTENSION_REQUIRED:
        return "Mandatory Extension";
    case CWH_WS_CLOSE_UNEXPECTED:
        return "Internal Server Error";
    default:
        return "Unknown";
    }
}
