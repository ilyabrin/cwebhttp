CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 -Iinclude -Itests
SRCS = src/cwebhttp.c src/memcheck.c
ASYNC_SRCS = src/async/loop.c src/async/epoll.c src/async/kqueue.c src/async/iocp.c src/async/wsapoll.c src/async/select.c src/async/nonblock.c src/async/client.c src/async/server.c

# OS detection
ifeq ($(OS),Windows_NT)
	# Windows
	CFLAGS += -D_WIN32
	LDFLAGS = -lws2_32 -lz
	MKDIR = if not exist $(subst /,\,$(1)) mkdir $(subst /,\,$(1))
	RM = cmd /c "if exist build rmdir /s /q build"
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

examples: build/examples/minimal_server$(EXE_EXT) build/examples/simple_client$(EXE_EXT) build/examples/hello_server$(EXE_EXT) build/examples/file_server$(EXE_EXT) build/examples/async_client$(EXE_EXT) build/examples/async_server$(EXE_EXT) build/examples/async_client_pool$(EXE_EXT) build/examples/memcheck_demo$(EXE_EXT) build/examples/json_api_server$(EXE_EXT) build/examples/static_file_server$(EXE_EXT) build/examples/benchmark_client$(EXE_EXT)

benchmarks: build/benchmarks/bench_parser$(EXE_EXT) build/benchmarks/bench_memory$(EXE_EXT) build/benchmarks/minimal_example$(EXE_EXT) build/benchmarks/bench_c10k$(EXE_EXT) build/benchmarks/bench_latency$(EXE_EXT) build/benchmarks/bench_async_throughput$(EXE_EXT)

test: build/tests/test_parse$(EXE_EXT) build/tests/test_url$(EXE_EXT) build/tests/test_chunked$(EXE_EXT) build/tests/test_memcheck$(EXE_EXT)
	$(call RUN_TEST,test_parse)
	$(call RUN_TEST,test_url)
	$(call RUN_TEST,test_chunked)
	$(call RUN_TEST,test_memcheck)

integration: build/tests/test_integration$(EXE_EXT)
	@echo "Running integration tests (requires internet connection)..."
	$(call RUN_TEST,test_integration)

async-tests: build/tests/test_async_loop$(EXE_EXT)
	@echo "Running async event loop tests..."
	$(call RUN_TEST,test_async_loop)

test-iocp: build/test_iocp_server$(EXE_EXT)
	@echo "Running IOCP server test (Windows only)..."
	.\build\test_iocp_server$(EXE_EXT)

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

build/examples/memcheck_demo$(EXE_EXT): examples/memcheck_demo.c src/memcheck.c
	@$(call MKDIR,build/examples)
	$(CC) $(CFLAGS) examples/memcheck_demo.c src/memcheck.c -o $@ $(LDFLAGS)

build/examples/json_api_server$(EXE_EXT): examples/json_api_server.c $(SRCS) $(ASYNC_SRCS)
	@$(call MKDIR,build/examples)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

build/examples/static_file_server$(EXE_EXT): examples/static_file_server.c $(SRCS) $(ASYNC_SRCS)
	@$(call MKDIR,build/examples)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

build/examples/benchmark_client$(EXE_EXT): examples/benchmark_client.c $(SRCS) $(ASYNC_SRCS)
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

build/tests/test_memcheck$(EXE_EXT): tests/test_memcheck.c tests/unity.c src/memcheck.c
	@$(call MKDIR,build/tests)
	$(CC) $(CFLAGS) tests/test_memcheck.c tests/unity.c src/memcheck.c -o $@ $(LDFLAGS)

build/benchmarks/bench_parser$(EXE_EXT): benchmarks/bench_parser.c $(SRCS)
	@$(call MKDIR,build/benchmarks)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

build/benchmarks/bench_memory$(EXE_EXT): benchmarks/bench_memory.c $(SRCS)
	@$(call MKDIR,build/benchmarks)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

build/benchmarks/minimal_example$(EXE_EXT): benchmarks/minimal_example.c $(SRCS)
	@$(call MKDIR,build/benchmarks)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

build/benchmarks/bench_c10k$(EXE_EXT): benchmarks/bench_c10k.c $(SRCS) $(ASYNC_SRCS)
	@$(call MKDIR,build/benchmarks)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

build/benchmarks/bench_latency$(EXE_EXT): benchmarks/bench_latency.c $(SRCS) $(ASYNC_SRCS)
	@$(call MKDIR,build/benchmarks)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

build/benchmarks/bench_async_throughput$(EXE_EXT): benchmarks/bench_async_throughput.c $(SRCS) $(ASYNC_SRCS)
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

build/examples/async_client_pool$(EXE_EXT): examples/async_client_pool.c $(SRCS) $(ASYNC_SRCS)
	@$(call MKDIR,build/examples)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

build/test_iocp_server$(EXE_EXT): test_iocp_server.c $(SRCS) $(ASYNC_SRCS)
	@$(call MKDIR,build)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	@$(RM)

# Docker targets
docker-build:
	docker build -t cwebhttp-test .

docker-test: docker-build
	docker run --rm cwebhttp-test

docker-c10k: docker-build
	docker run --rm --ulimit nofile=65536:65536 --ulimit nproc=32768:32768 --memory=2g cwebhttp-test ./build/benchmarks/bench_c10k

docker-shell: docker-build
	docker run --rm -it cwebhttp-test /bin/bash

.PHONY: all examples benchmarks test tests integration async-tests clean docker-build docker-test docker-shell