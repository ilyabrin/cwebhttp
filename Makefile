CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 -Iinclude -Itests
SRCS = src/cwebhttp.c
ASYNC_SRCS = src/async/loop.c src/async/epoll.c src/async/kqueue.c src/async/iocp.c src/async/select.c src/async/nonblock.c src/async/client.c src/async/server.c

# OS detection
ifeq ($(OS),Windows_NT)
	# Windows
	CFLAGS += -D_WIN32
	LDFLAGS = -lws2_32 -lz
	MKDIR = if not exist $(subst /,\,$(1)) mkdir $(subst /,\,$(1))
	RM = if exist build rmdir /s /q build
	EXE_EXT = .exe
	RUN_TEST = .\build\tests\$(1).exe
else
	# Unix-like (Linux, macOS, etc.)
	UNAME_S := $(shell uname -s)
	LDFLAGS = -lz
	MKDIR = mkdir -p $(1)
	RM = rm -rf build
	EXE_EXT =
	RUN_TEST = ./build/tests/$(1)
endif

all: examples build/tests/test_parse$(EXE_EXT) build/tests/test_url$(EXE_EXT) build/tests/test_chunked$(EXE_EXT)

examples: build/examples/minimal_server$(EXE_EXT) build/examples/simple_client$(EXE_EXT) build/examples/hello_server$(EXE_EXT) build/examples/file_server$(EXE_EXT) build/examples/async_client$(EXE_EXT) build/examples/async_server$(EXE_EXT)

benchmarks: build/benchmarks/bench_parser$(EXE_EXT) build/benchmarks/bench_memory$(EXE_EXT) build/benchmarks/minimal_example$(EXE_EXT)

test: build/tests/test_parse$(EXE_EXT) build/tests/test_url$(EXE_EXT) build/tests/test_chunked$(EXE_EXT)
	$(call RUN_TEST,test_parse)
	$(call RUN_TEST,test_url)
	$(call RUN_TEST,test_chunked)

integration: build/tests/test_integration$(EXE_EXT)
	@echo "Running integration tests (requires internet connection)..."
	$(call RUN_TEST,test_integration)

async-tests: build/tests/test_async_loop$(EXE_EXT)
	@echo "Running async event loop tests..."
	$(call RUN_TEST,test_async_loop)

tests: test

build/examples/minimal_server$(EXE_EXT): examples/minimal_server.c $(SRCS)
	@$(call MKDIR,build/examples)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

build/examples/simple_client$(EXE_EXT): examples/simple_client.c $(SRCS)
	@$(call MKDIR,build/examples)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

build/examples/hello_server$(EXE_EXT): examples/hello_server.c $(SRCS)
	@$(call MKDIR,build/examples)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

build/examples/file_server$(EXE_EXT): examples/file_server.c $(SRCS)
	@$(call MKDIR,build/examples)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

build/tests/test_parse$(EXE_EXT): tests/test_parse.c tests/unity.c $(SRCS)
	@$(call MKDIR,build/tests)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

build/tests/test_url$(EXE_EXT): tests/test_url.c tests/unity.c $(SRCS)
	@$(call MKDIR,build/tests)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

build/tests/test_chunked$(EXE_EXT): tests/test_chunked.c tests/unity.c $(SRCS)
	@$(call MKDIR,build/tests)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

build/tests/test_integration$(EXE_EXT): tests/test_integration.c tests/unity.c $(SRCS)
	@$(call MKDIR,build/tests)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

build/benchmarks/bench_parser$(EXE_EXT): benchmarks/bench_parser.c $(SRCS)
	@$(call MKDIR,build/benchmarks)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

build/benchmarks/bench_memory$(EXE_EXT): benchmarks/bench_memory.c $(SRCS)
	@$(call MKDIR,build/benchmarks)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

build/benchmarks/minimal_example$(EXE_EXT): benchmarks/minimal_example.c $(SRCS)
	@$(call MKDIR,build/benchmarks)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

build/tests/test_async_loop$(EXE_EXT): tests/test_async_loop.c tests/unity.c $(SRCS) $(ASYNC_SRCS)
	@$(call MKDIR,build/tests)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

build/examples/async_client$(EXE_EXT): examples/async_client.c $(SRCS) $(ASYNC_SRCS)
	@$(call MKDIR,build/examples)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

build/examples/async_server$(EXE_EXT): examples/async_server.c $(SRCS) $(ASYNC_SRCS)
	@$(call MKDIR,build/examples)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	@$(RM)

# Docker targets
docker-build:
	docker build -t cwebhttp-test .

docker-test: docker-build
	docker run --rm cwebhttp-test

docker-shell: docker-build
	docker run --rm -it cwebhttp-test /bin/bash

.PHONY: all examples benchmarks test tests integration async-tests clean docker-build docker-test docker-shell