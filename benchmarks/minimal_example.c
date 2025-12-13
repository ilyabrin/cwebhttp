// Minimal example - just parsing, no networking
// Used to measure core library size

#include "cwebhttp.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    const char *request = "GET /hello HTTP/1.1\r\nHost: example.com\r\n\r\n";

    char buf[256];
    strcpy(buf, request);

    cwh_request_t req = {0};
    if (cwh_parse_req(buf, strlen(buf), &req) == CWH_OK)
    {
        printf("Parsed successfully\n");
    }

    return 0;
}
