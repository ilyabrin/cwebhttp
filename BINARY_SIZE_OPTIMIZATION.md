# Binary Size Optimization Guide

## Overview

cwebhttp is designed to be modular and lightweight. You can significantly reduce binary size by disabling unused features through compile-time flags.

## Quick Start

### Build Presets

The easiest way to optimize binary size is to use one of the provided build presets:

```bash
# Minimal build: ~20KB (parsing only)
gcc -DCWEBHTTP_BUILD_MINIMAL -O2 -Iinclude your_app.c src/cwebhttp.c -o app

# Client-only: ~40KB (no server features)
gcc -DCWEBHTTP_BUILD_CLIENT_ONLY -O2 -Iinclude your_app.c src/cwebhttp.c src/memcheck.c -o app -lz -lws2_32

# Server-only: ~40KB (no client features)
gcc -DCWEBHTTP_BUILD_SERVER_ONLY -O2 -Iinclude your_app.c src/cwebhttp.c src/memcheck.c -o app -lz -lws2_32

# Full build: ~85KB (all features)
gcc -O2 -Iinclude your_app.c src/cwebhttp.c src/memcheck.c src/log.c src/error.c -o app -lz -lws2_32
```

Or use the build script on Windows:

```batch
build_config.bat minimal    # ~20KB
build_config.bat client     # ~40KB
build_config.bat server     # ~40KB
build_config.bat full       # ~85KB
```

## Feature Flags

All feature flags are defined in `include/cwebhttp_config.h`. You can override them at compile time:

### Core Features (Cannot be Disabled)

- HTTP/1.1 request/response parsing (zero-allocation)
- Basic TCP client/server
- URL parsing

### Optional Features

| Feature | Flag | Size Impact | Description |
|---------|------|-------------|-------------|
| **TLS/HTTPS** | `CWEBHTTP_ENABLE_TLS` | +150KB | HTTPS client/server, certificate validation, SNI |
| **Async I/O** | `CWEBHTTP_ENABLE_ASYNC` | +15-20KB | Event loop, async client/server, C10K+ capability |
| **WebSocket** | `CWEBHTTP_ENABLE_WEBSOCKET` | +10KB | WebSocket client/server, frame encoding/decoding |
| **Compression** | `CWEBHTTP_ENABLE_COMPRESSION` | +5KB | gzip/deflate decompression, Accept-Encoding |
| **Cookies** | `CWEBHTTP_ENABLE_COOKIES` | +5KB | Cookie jar, Set-Cookie parsing, automatic headers |
| **Redirects** | `CWEBHTTP_ENABLE_REDIRECTS` | +3KB | Automatic redirect following (301, 302, 307, 308) |
| **Connection Pool** | `CWEBHTTP_ENABLE_CONNECTION_POOL` | +5KB | Keep-alive connections, connection reuse |
| **Chunked Encoding** | `CWEBHTTP_ENABLE_CHUNKED` | +2KB | Chunked transfer encoding/decoding |
| **File Serving** | `CWEBHTTP_ENABLE_FILE_SERVING` | +4KB | Static file server, MIME types, Range requests |
| **Logging** | `CWEBHTTP_ENABLE_LOGGING` | +3KB | Leveled logging system (DEBUG/INFO/WARN/ERROR) |
| **Memory Check** | `CWEBHTTP_ENABLE_MEMCHECK` | +4KB | Leak detection, allocation tracking |
| **Advanced Errors** | `CWEBHTTP_ENABLE_ADVANCED_ERRORS` | +2KB | Error context, location tracking |

## Custom Configuration Examples

### Example 1: Embedded HTTP Parser Only

Perfect for embedded systems that only need to parse HTTP messages:

```c
// Compile flags
gcc -DCWEBHTTP_BUILD_MINIMAL \
    -Os -Iinclude \
    your_parser.c src/cwebhttp.c \
    -o parser
```

**Result:** ~15-20KB binary

**Features:**
- ✅ HTTP request/response parsing
- ✅ Zero-allocation parsing
- ❌ No networking
- ❌ No allocations
- ❌ No external dependencies

### Example 2: Lightweight HTTP Client

For applications that just need a simple HTTP client:

```c
// Disable features you don't need
gcc -DCWEBHTTP_ENABLE_WEBSOCKET=0 \
    -DCWEBHTTP_ENABLE_FILE_SERVING=0 \
    -DCWEBHTTP_ENABLE_MEMCHECK=0 \
    -DCWEBHTTP_ENABLE_LOGGING=0 \
    -O2 -Iinclude \
    client.c src/cwebhttp.c src/memcheck.c src/log.c src/error.c \
    -o client -lz -lws2_32
```

**Result:** ~35-40KB binary

**Features:**
- ✅ HTTP client (GET, POST, PUT, DELETE)
- ✅ Connection pooling
- ✅ Cookies
- ✅ Redirects
- ✅ Compression
- ❌ No WebSocket
- ❌ No file serving
- ❌ No memory checking

### Example 3: High-Performance Server

For production servers that need async I/O but not client features:

```c
gcc -DCWEBHTTP_BUILD_SERVER_ONLY \
    -O3 -flto -Iinclude \
    server.c src/cwebhttp.c src/memcheck.c src/log.c src/error.c \
    src/async/*.c \
    -o server -lz -lws2_32
```

**Result:** ~45-50KB binary

**Features:**
- ✅ Async server (epoll/kqueue/IOCP)
- ✅ Static file serving
- ✅ WebSocket support
- ✅ Logging
- ❌ No client features
- ❌ No cookies
- ❌ No redirects

### Example 4: HTTPS-Only Client

For secure applications:

```c
gcc -DCWEBHTTP_ENABLE_TLS=1 \
    -DCWEBHTTP_ENABLE_WEBSOCKET=0 \
    -DCWEBHTTP_ENABLE_FILE_SERVING=0 \
    -O2 -Iinclude \
    https_client.c src/cwebhttp.c src/tls_mbedtls.c src/memcheck.c \
    -o https_client \
    -lz -lws2_32 -lmbedtls -lmbedx509 -lmbedcrypto
```

**Result:** ~60KB + mbedTLS (~150KB) = ~210KB total

## Advanced Optimization Techniques

### 1. Link-Time Optimization (LTO)

```bash
gcc -flto -O3 -Iinclude your_app.c src/*.c -o app
```

**Benefit:** 10-20% size reduction

### 2. Strip Symbols

```bash
strip app
# Or during compilation:
gcc -s -O2 -Iinclude your_app.c src/*.c -o app
```

**Benefit:** 30-40% size reduction

### 3. Size Optimization Flags

```bash
gcc -Os -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -Iinclude your_app.c src/*.c -o app
```

**Benefit:** 15-25% size reduction

### 4. Combined (Maximum Size Reduction)

```bash
gcc -DCWEBHTTP_BUILD_MINIMAL \
    -Os -flto \
    -ffunction-sections -fdata-sections \
    -Wl,--gc-sections \
    -s \
    -Iinclude your_app.c src/cwebhttp.c -o app
```

**Result:** Can achieve <15KB for minimal builds

## Configuration Constants

You can also tune buffer sizes and limits:

```c
// Maximum headers per request/response
#define CWEBHTTP_MAX_HEADERS 32        // Default: 32, Range: 8-128

// Maximum URL length
#define CWEBHTTP_MAX_URL_LENGTH 2048   // Default: 2048, Range: 256-8192

// Connection timeout (milliseconds)
#define CWEBHTTP_DEFAULT_TIMEOUT 30000 // Default: 30000 (30s)

// Maximum redirects to follow
#define CWEBHTTP_MAX_REDIRECTS 10      // Default: 10, Range: 0-50

// Connection pool size
#define CWEBHTTP_POOL_SIZE 50          // Default: 50, Range: 1-1000

// Connection pool idle timeout (seconds)
#define CWEBHTTP_POOL_IDLE_TIMEOUT 300 // Default: 300 (5 min)
```

## Size Comparison

Here's a real-world comparison of different configurations:

| Configuration | Binary Size | Features |
|---------------|-------------|----------|
| **Minimal** | 15-20 KB | Parser only |
| **Client-only** | 35-40 KB | HTTP client, no server |
| **Server-only** | 40-45 KB | HTTP server, no client |
| **Full (no TLS)** | 80-85 KB | All features except TLS |
| **Full + TLS** | 230-250 KB | All features with HTTPS |
| **libcurl** | 600+ KB | Equivalent functionality |

## Best Practices

1. **Start minimal, add features as needed** - Begin with `CWEBHTTP_BUILD_MINIMAL` and enable only what you use

2. **Use build presets** - The provided presets (`MINIMAL`, `CLIENT_ONLY`, `SERVER_ONLY`) are well-tested

3. **Profile your binary** - Use `size` command to see what's using space:
   ```bash
   size your_app
   ```

4. **Test after optimization** - Always run your tests after changing feature flags

5. **Document your configuration** - Keep track of which flags you're using in your build system

## Troubleshooting

### "Undefined reference" errors

If you get linker errors after disabling features, make sure you're not calling disabled APIs:

```c
#if CWEBHTTP_ENABLE_COOKIES
    cwh_cookie_jar_init();
#endif
```

### Binary size not decreasing

1. Make sure you're using optimization flags (`-O2` or `-Os`)
2. Strip symbols with `strip` or `-s` flag
3. Use LTO with `-flto`
4. Check that disabled code isn't being pulled in

### Feature dependencies

Some features depend on others:
- WebSocket requires Async I/O
- Connection pool works better with Cookies

The library will warn you at compile time about dependency issues.

## Getting Help

See full documentation at: https://github.com/yourusername/cwebhttp

For questions and issues: https://github.com/yourusername/cwebhttp/issues
