// Fuzzing target for URL parser
// Build with: clang -fsanitize=fuzzer,address -O2 -Iinclude fuzz_url_parser.c src/cwebhttp.c -o fuzz_url -lz

#include "cwebhttp.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Fuzzer entry point
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    // Limit input size
    if (size < 7 || size > 2048)
    {
        return 0;
    }

    // Make a mutable copy
    char *buf = (char *)malloc(size + 1);
    if (!buf)
    {
        return 0;
    }

    memcpy(buf, data, size);
    buf[size] = '\0';

    // Try to parse as URL
    cwh_url_t url = {0};
    cwh_error_t err = cwh_parse_url(buf, size, &url);

    // If parsing succeeded, validate the structure
    if (err == CWH_OK && url.is_valid)
    {
        // Access fields to ensure they're valid pointers within buffer
        if (url.scheme)
        {
            volatile char c = *url.scheme;
            (void)c;
        }
        if (url.host)
        {
            volatile char c = *url.host;
            (void)c;
        }
        if (url.path)
        {
            volatile char c = *url.path;
            (void)c;
        }
        if (url.query)
        {
            volatile char c = *url.query;
            (void)c;
        }
        if (url.fragment)
        {
            volatile char c = *url.fragment;
            (void)c;
        }

        // Validate port
        volatile int port = url.port;
        (void)port;
    }

    free(buf);
    return 0;
}

// AFL fuzzer main
#ifdef __AFL_COMPILER
int main(int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    FILE *f = fopen(argv[1], "rb");
    if (!f)
    {
        perror("fopen");
        return 1;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size < 0 || size > 2048)
    {
        fclose(f);
        return 1;
    }

    uint8_t *buf = malloc(size);
    if (!buf)
    {
        fclose(f);
        return 1;
    }

    fread(buf, 1, size, f);
    fclose(f);

    LLVMFuzzerTestOneInput(buf, size);

    free(buf);
    return 0;
}
#endif
