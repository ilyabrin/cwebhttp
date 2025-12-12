CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 -Iinclude -Itests
SRCS = src/cwebhttp.c

all: examples tests

examples: build/examples/minimal_server build/examples/simple_client

tests: build/tests/test_parse
    ./build/tests/test_parse

build/examples/minimal_server: examples/minimal_server.c $(SRCS)
    mkdir -p build/examples
    $(CC) $(CFLAGS) $^ -o $@

build/examples/simple_client: examples/simple_client.c $(SRCS)
    mkdir -p build/examples
    $(CC) $(CFLAGS) $^ -o $@

build/tests/test_parse: tests/test_parse.c tests/unity.c $(SRCS)
    mkdir -p build/tests
    $(CC) $(CFLAGS) $^ -o $@

clean:
    rm -rf build

.PHONY: all examples tests clean