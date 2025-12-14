// Simple async test - minimal debugging
#include <stdio.h>
#include <stdlib.h>

// Test if we can even include the header
#include "cwebhttp_async.h"

int main(void)
{
    printf("Starting minimal async test...\n");

    printf("Step 1: Attempting to create event loop...\n");
    fflush(stdout);

    cwh_loop_t *loop = cwh_loop_new();

    if (!loop)
    {
        printf("ERROR: cwh_loop_new() returned NULL\n");
        return 1;
    }

    printf("Step 2: Event loop created successfully!\n");

    const char *backend = cwh_loop_backend(loop);
    printf("Step 3: Backend: %s\n", backend ? backend : "NULL");

    printf("Step 4: Freeing event loop...\n");
    cwh_loop_free(loop);

    printf("Step 5: Success!\n");

    return 0;
}
