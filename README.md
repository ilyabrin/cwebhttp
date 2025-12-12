# cwebhttp

Лёгкая, современная HTTP/1.1+ библиотека на чистом C. Zero-allocation парсинг, async I/O, <80KB.

[![CI](https://github.com/ilyabrin/cwebhttp/actions/workflows/ci.yml/badge.svg)](https://github.com/ilyabrin/cwebhttp/actions)

## Build & Test

```bash
make test      # запустит тесты
make examples  # соберёт примеры
./build/examples/minimal_server  # запусти сервер (dummy пока)
```

## Features

- HTTP/1.1+
- Zero-allocation парсинг HTTP сообщений
- Асинхронный ввод-вывод (epoll/kqueue/iocp)
- Поддержка TLS (через OpenSSL или mbedTLS)
- Поддержка HTTP/2 (через nghttp2)
- Малый размер бинарника (<80KB без TLS и HTTP/2)
- Кроссплатформенность (Linux, macOS, Windows)
