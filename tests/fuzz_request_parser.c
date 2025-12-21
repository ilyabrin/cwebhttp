// Fuzzing target for HTTP request parser
// Build with: clang -fsanitize=fuzzer,address -O2 -Iinclude fuzz_request_parser.c src/cwebhttp.c -o fuzz_request -lz

#include "cwebhttp.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Fuzzer entry point
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    // Limit input size to avoid timeouts
    if (size < 10 || size > 8192)
    {
        return 0;
    }

    // Make a mutable copy for zero-allocation parsing
    char *buf = (char *)malloc(size + 1);
    if (!buf)
    {
        return 0;
    }

    memcpy(buf, data, size);
    buf[size] = '\0';

    // Try to parse as HTTP request
    cwh_request_t req = {0};
    cwh_parse_req(buf, size, &req);

    // If parsing succeeded, validate the structure
    if (req.is_valid)
    {
        // Access fields to ensure they're valid pointers
        if (req.method_str)
        {
            volatile size_t len = strlen(req.method_str);
            (void)len;
        }
        if (req.path)
        {
            volatile size_t len = strlen(req.path);
            (void)len;
        }
        if (req.query)
        {
            volatile size_t len = strlen(req.query);
            (void)len;
        }

        // Validate headers
        for (size_t i = 0; i < req.num_headers && i < 32; i++)
        {
            if (req.headers[i * 2])
            {
                volatile size_t len = strlen(req.headers[i * 2]);
                (void)len;
            }
            if (req.headers[i * 2 + 1])
            {
                volatile size_t len = strlen(req.headers[i * 2 + 1]);
                (void)len;
            }
        }
    }

    free(buf);
    return 0;
}

// AFL fuzzer main (when compiled with afl-gcc)
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

    if (size < 0 || size > 8192)
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
