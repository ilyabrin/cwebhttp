// cwebhttp Configuration & Feature Flags
// This file controls which features are compiled into the library
// Use these flags to optimize binary size by disabling unused features

#ifndef CWEBHTTP_CONFIG_H
#define CWEBHTTP_CONFIG_H

// ============================================================================
// Core Features (always enabled, cannot be disabled)
// ============================================================================
// - HTTP/1.1 request/response parsing (zero-allocation)
// - Basic TCP client/server
// - URL parsing

// ============================================================================
// Optional Features (can be disabled to reduce binary size)
// ============================================================================

// TLS/HTTPS Support (requires mbedTLS)
// Adds: ~150KB (mbedTLS library)
// Enables: HTTPS client/server, certificate validation, SNI, session resumption
#ifndef CWEBHTTP_ENABLE_TLS
#define CWEBHTTP_ENABLE_TLS 0
#endif

// Async I/O Support (epoll/kqueue/IOCP)
// Adds: ~15-20KB
// Enables: Event loop, async client/server, C10K+ capability
#ifndef CWEBHTTP_ENABLE_ASYNC
#define CWEBHTTP_ENABLE_ASYNC 1
#endif

// WebSocket Support
// Adds: ~10KB
// Enables: WebSocket client/server, frame encoding/decoding
#ifndef CWEBHTTP_ENABLE_WEBSOCKET
#define CWEBHTTP_ENABLE_WEBSOCKET 1
#endif

// Compression Support (gzip/deflate)
// Adds: ~5KB (zlib already linked for HTTP)
// Enables: Automatic response decompression, Accept-Encoding headers
#ifndef CWEBHTTP_ENABLE_COMPRESSION
#define CWEBHTTP_ENABLE_COMPRESSION 1
#endif

// Cookie Support
// Adds: ~5KB
// Enables: Cookie jar, Set-Cookie parsing, automatic Cookie headers
#ifndef CWEBHTTP_ENABLE_COOKIES
#define CWEBHTTP_ENABLE_COOKIES 1
#endif

// Redirect Support (301, 302, 307, 308)
// Adds: ~3KB
// Enables: Automatic redirect following, circular detection
#ifndef CWEBHTTP_ENABLE_REDIRECTS
#define CWEBHTTP_ENABLE_REDIRECTS 1
#endif

// Connection Pool Support
// Adds: ~5KB
// Enables: Keep-alive connections, connection reuse, pool management
#ifndef CWEBHTTP_ENABLE_CONNECTION_POOL
#define CWEBHTTP_ENABLE_CONNECTION_POOL 1
#endif

// Chunked Transfer Encoding
// Adds: ~2KB
// Enables: Chunked request/response encoding/decoding
#ifndef CWEBHTTP_ENABLE_CHUNKED
#define CWEBHTTP_ENABLE_CHUNKED 1
#endif

// Static File Serving (MIME types, Range requests)
// Adds: ~4KB
// Enables: File server, MIME type detection, HTTP 206 Partial Content
#ifndef CWEBHTTP_ENABLE_FILE_SERVING
#define CWEBHTTP_ENABLE_FILE_SERVING 1
#endif

// Logging System
// Adds: ~3KB
// Enables: Leveled logging (DEBUG/INFO/WARN/ERROR), custom handlers
#ifndef CWEBHTTP_ENABLE_LOGGING
#define CWEBHTTP_ENABLE_LOGGING 1
#endif

// Memory Leak Detection
// Adds: ~4KB
// Enables: Allocation tracking, leak detection, memory statistics
#ifndef CWEBHTTP_ENABLE_MEMCHECK
#define CWEBHTTP_ENABLE_MEMCHECK 1
#endif

// Advanced Error Handling
// Adds: ~2KB
// Enables: Error context, location tracking, detailed messages
#ifndef CWEBHTTP_ENABLE_ADVANCED_ERRORS
#define CWEBHTTP_ENABLE_ADVANCED_ERRORS 1
#endif

// ============================================================================
// Performance Tuning Options
// ============================================================================

// Maximum number of headers per request/response
#ifndef CWEBHTTP_MAX_HEADERS
#define CWEBHTTP_MAX_HEADERS 32
#endif

// Maximum URL length
#ifndef CWEBHTTP_MAX_URL_LENGTH
#define CWEBHTTP_MAX_URL_LENGTH 2048
#endif

// Default connection timeout (milliseconds)
#ifndef CWEBHTTP_DEFAULT_TIMEOUT
#define CWEBHTTP_DEFAULT_TIMEOUT 30000
#endif

// Maximum redirects to follow
#ifndef CWEBHTTP_MAX_REDIRECTS
#define CWEBHTTP_MAX_REDIRECTS 10
#endif

// Connection pool size
#ifndef CWEBHTTP_POOL_SIZE
#define CWEBHTTP_POOL_SIZE 50
#endif

// Connection pool idle timeout (seconds)
#ifndef CWEBHTTP_POOL_IDLE_TIMEOUT
#define CWEBHTTP_POOL_IDLE_TIMEOUT 300
#endif

// ============================================================================
// Build Configuration Presets
// ============================================================================

#ifdef CWEBHTTP_BUILD_MINIMAL
// Minimal build: Only core HTTP/1.1 parsing (~20KB)
#undef CWEBHTTP_ENABLE_ASYNC
#undef CWEBHTTP_ENABLE_WEBSOCKET
#undef CWEBHTTP_ENABLE_COMPRESSION
#undef CWEBHTTP_ENABLE_COOKIES
#undef CWEBHTTP_ENABLE_REDIRECTS
#undef CWEBHTTP_ENABLE_CONNECTION_POOL
#undef CWEBHTTP_ENABLE_CHUNKED
#undef CWEBHTTP_ENABLE_FILE_SERVING
#undef CWEBHTTP_ENABLE_LOGGING
#undef CWEBHTTP_ENABLE_MEMCHECK
#undef CWEBHTTP_ENABLE_ADVANCED_ERRORS

#define CWEBHTTP_ENABLE_ASYNC 0
#define CWEBHTTP_ENABLE_WEBSOCKET 0
#define CWEBHTTP_ENABLE_COMPRESSION 0
#define CWEBHTTP_ENABLE_COOKIES 0
#define CWEBHTTP_ENABLE_REDIRECTS 0
#define CWEBHTTP_ENABLE_CONNECTION_POOL 0
#define CWEBHTTP_ENABLE_CHUNKED 0
#define CWEBHTTP_ENABLE_FILE_SERVING 0
#define CWEBHTTP_ENABLE_LOGGING 0
#define CWEBHTTP_ENABLE_MEMCHECK 0
#define CWEBHTTP_ENABLE_ADVANCED_ERRORS 0
#endif

#ifdef CWEBHTTP_BUILD_CLIENT_ONLY
// Client-only build: No server features (~40KB)
#undef CWEBHTTP_ENABLE_FILE_SERVING
#define CWEBHTTP_ENABLE_FILE_SERVING 0
#endif

#ifdef CWEBHTTP_BUILD_SERVER_ONLY
// Server-only build: No client features (~40KB)
#undef CWEBHTTP_ENABLE_COMPRESSION
#undef CWEBHTTP_ENABLE_COOKIES
#undef CWEBHTTP_ENABLE_REDIRECTS
#undef CWEBHTTP_ENABLE_CONNECTION_POOL

#define CWEBHTTP_ENABLE_COMPRESSION 0
#define CWEBHTTP_ENABLE_COOKIES 0
#define CWEBHTTP_ENABLE_REDIRECTS 0
#define CWEBHTTP_ENABLE_CONNECTION_POOL 0
#endif

// ============================================================================
// Feature Dependency Validation
// ============================================================================

// Async server requires async support
#if CWEBHTTP_ENABLE_ASYNC == 0 && defined(CWEBHTTP_NEED_ASYNC_SERVER)
#error "Async server requires CWEBHTTP_ENABLE_ASYNC=1"
#endif

// WebSocket requires async support
#if CWEBHTTP_ENABLE_WEBSOCKET && !CWEBHTTP_ENABLE_ASYNC
#error "WebSocket requires CWEBHTTP_ENABLE_ASYNC=1"
#endif

// Connection pool requires cookies for session management
#if CWEBHTTP_ENABLE_CONNECTION_POOL && !CWEBHTTP_ENABLE_COOKIES
#warning "Connection pool works better with CWEBHTTP_ENABLE_COOKIES=1"
#endif

// ============================================================================
// Build Information
// ============================================================================

// Generate build info string
#define CWEBHTTP_FEATURE_TLS (CWEBHTTP_ENABLE_TLS ? "TLS " : "")
#define CWEBHTTP_FEATURE_ASYNC (CWEBHTTP_ENABLE_ASYNC ? "ASYNC " : "")
#define CWEBHTTP_FEATURE_WS (CWEBHTTP_ENABLE_WEBSOCKET ? "WS " : "")
#define CWEBHTTP_FEATURE_COMPRESSION (CWEBHTTP_ENABLE_COMPRESSION ? "GZIP " : "")

#define CWEBHTTP_BUILD_INFO \
    "cwebhttp " CWEBHTTP_VERSION " [" \
    CWEBHTTP_FEATURE_TLS \
    CWEBHTTP_FEATURE_ASYNC \
    CWEBHTTP_FEATURE_WS \
    CWEBHTTP_FEATURE_COMPRESSION \
    "]"

#endif // CWEBHTTP_CONFIG_H
