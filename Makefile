CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 -Iinclude -Itests
SRCS = src/cwebhttp.c

# OS detection
UNAME_S := $(shell uname -s 2>/dev/null || echo Windows)
ifeq ($(UNAME_S),Windows)
	CFLAGS += -D_WIN32
	LDFLAGS = -lws2_32
	MKDIR = mkdir -p $(1)
	RM = rm -rf
	EXE_EXT = .exe
else ifeq ($(OS),Windows_NT)
	CFLAGS += -D_WIN32
	LDFLAGS = -lws2_32
	MKDIR = mkdir -p $(1)
	RM = rm -rf
	EXE_EXT = .exe
else
	ifeq ($(UNAME_S),Linux)
		LDFLAGS =
	endif
	ifeq ($(UNAME_S),Darwin)
		LDFLAGS =
	endif
	MKDIR = mkdir -p $(1)
	RM = rm -rf
	EXE_EXT =
endif

all: examples build/tests/test_parse$(EXE_EXT)

examples: build/examples/minimal_server$(EXE_EXT) build/examples/simple_client$(EXE_EXT)

test: build/tests/test_parse$(EXE_EXT)
	./build/tests/test_parse$(EXE_EXT)

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

clean:
	$(RM) build

.PHONY: all examples test tests clean