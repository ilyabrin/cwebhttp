// Fuzzing target for HTTP response parser
// Build with: clang -fsanitize=fuzzer,address -O2 -Iinclude fuzz_response_parser.c src/cwebhttp.c -o fuzz_response -lz

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
    if (size < 10 || size > 8192)
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

    // Try to parse as HTTP response
    cwh_response_t res = {0};
    cwh_parse_res(buf, size, &res);

    // If parsing succeeded (status > 0), validate the structure
    if (res.status > 0)
    {
        // Access status field
        volatile int status = res.status;
        (void)status;

        // Validate headers
        for (size_t i = 0; i < res.num_headers && i < 32; i++)
        {
            if (res.headers[i * 2])
            {
                volatile size_t len = strlen(res.headers[i * 2]);
                (void)len;
            }
            if (res.headers[i * 2 + 1])
            {
                volatile size_t len = strlen(res.headers[i * 2 + 1]);
                (void)len;
            }
        }

        // Check body if present
        if (res.body && res.body_len > 0)
        {
            volatile char first = res.body[0];
            (void)first;
        }
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
