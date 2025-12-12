CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 -Iinclude -Itests
SRCS = src/cwebhttp.c

# OS detection
ifeq ($(OS),Windows_NT)
	# Windows
	CFLAGS += -D_WIN32
	LDFLAGS = -lws2_32
	MKDIR = if not exist $(subst /,\,$(1)) mkdir $(subst /,\,$(1))
	RM = if exist build rmdir /s /q build
	EXE_EXT = .exe
else
	# Unix-like (Linux, macOS, etc.)
	UNAME_S := $(shell uname -s)
	LDFLAGS = 
	MKDIR = mkdir -p $(1)
	RM = rm -rf build
	EXE_EXT = 
endif

all: examples build/tests/test_parse$(EXE_EXT) build/tests/test_url$(EXE_EXT)

examples: build/examples/minimal_server$(EXE_EXT) build/examples/simple_client$(EXE_EXT)

test: build/tests/test_parse$(EXE_EXT) build/tests/test_url$(EXE_EXT)
	./build/tests/test_parse$(EXE_EXT)
	./build/tests/test_url$(EXE_EXT)

tests: test

build/examples/minimal_server$(EXE_EXT): examples/minimal_server.c $(SRCS)
	@$(call MKDIR,build/examples)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

build/examples/simple_client$(EXE_EXT): examples/simple_client.c $(SRCS)
	@$(call MKDIR,build/examples)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

build/tests/test_parse$(EXE_EXT): tests/test_parse.c tests/unity.c $(SRCS)
	@$(call MKDIR,build/tests)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

build/tests/test_url$(EXE_EXT): tests/test_url.c tests/unity.c $(SRCS)
	@$(call MKDIR,build/tests)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	@$(RM)

.PHONY: all examples test tests clean