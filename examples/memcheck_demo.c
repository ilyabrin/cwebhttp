// memcheck_demo.c - Demonstration of memory leak detection

#include <stdio.h>
#include <string.h>

// Enable memory checking AFTER system headers
#define CWH_MEMCHECK_ENABLED 1
#include "cwebhttp_memcheck.h"

// Example 1: Function with no leaks
void example_no_leak(void)
{
    printf("\n=== Example 1: No Memory Leaks ===\n");

    char *buffer = malloc(1024);
    if (buffer)
    {
        strcpy(buffer, "Hello, World!");
        printf("Buffer content: %s\n", buffer);
        free(buffer); // Proper cleanup
    }

    printf("Example 1 complete - no leaks\n");
}

// Example 2: Function with intentional leak
void example_with_leak(void)
{
    printf("\n=== Example 2: Intentional Memory Leak ===\n");

    char *leak1 = malloc(100);
    char *leak2 = malloc(200);

    if (leak1 && leak2)
    {
        strcpy(leak1, "This will leak!");
        strcpy(leak2, "This will also leak!");
        printf("Created two allocations that won't be freed\n");
    }

    // Intentionally NOT freeing leak1 and leak2
    printf("Example 2 complete - 2 leaks created\n");
}

// Example 3: Realloc usage
void example_realloc(void)
{
    printf("\n=== Example 3: Realloc Tracking ===\n");

    char *buffer = malloc(100);
    if (buffer)
    {
        strcpy(buffer, "Initial size");
        printf("Initial: %s\n", buffer);

        // Grow the buffer
        buffer = realloc(buffer, 200);
        if (buffer)
        {
            strcat(buffer, " - expanded!");
            printf("After realloc: %s\n", buffer);
            free(buffer); // Proper cleanup
        }
    }

    printf("Example 3 complete - no leaks\n");
}

// Example 4: Check memory statistics
void example_statistics(void)
{
    printf("\n=== Example 4: Memory Statistics ===\n");

    // Allocate some memory
    void *ptrs[10];
    for (int i = 0; i < 10; i++)
    {
        ptrs[i] = malloc((i + 1) * 100);
    }

    // Get statistics
    cwh_memcheck_stats_t stats = cwh_memcheck_get_stats();

    printf("Current allocations: %llu\n", (unsigned long long)stats.current_allocations);
    printf("Current memory usage: %llu bytes\n", (unsigned long long)stats.current_bytes);
    printf("Peak allocations: %llu\n", (unsigned long long)stats.peak_allocations);
    printf("Peak memory usage: %llu bytes\n", (unsigned long long)stats.peak_bytes);

    // Free half
    for (int i = 0; i < 5; i++)
    {
        free(ptrs[i]);
    }

    stats = cwh_memcheck_get_stats();
    printf("\nAfter freeing 5 allocations:\n");
    printf("Current allocations: %llu\n", (unsigned long long)stats.current_allocations);
    printf("Current memory usage: %llu bytes\n", (unsigned long long)stats.current_bytes);
    printf("Peak still at: %llu allocations, %llu bytes\n",
           (unsigned long long)stats.peak_allocations, (unsigned long long)stats.peak_bytes);

    // Clean up the rest
    for (int i = 5; i < 10; i++)
    {
        free(ptrs[i]);
    }

    printf("Example 4 complete\n");
}

// Example 5: Process memory info
void example_process_memory(void)
{
    printf("\n=== Example 5: Process Memory Info ===\n");

    cwh_process_memory_t mem;
    if (cwh_memcheck_get_process_memory(&mem) == 0)
    {
        printf("Resident Set Size (RSS): %llu bytes (%.2f MB)\n",
               (unsigned long long)mem.rss, mem.rss / (1024.0 * 1024.0));
        printf("Virtual Memory Size: %llu bytes (%.2f MB)\n",
               (unsigned long long)mem.vsize, mem.vsize / (1024.0 * 1024.0));
        printf("Page faults: %llu\n", (unsigned long long)mem.page_faults);
    }
    else
    {
        printf("Process memory info not available on this platform\n");
    }
}

int main(void)
{
    printf("========================================\n");
    printf("Memory Leak Detection Demo\n");
    printf("========================================\n");

    // Initialize memory checker
    CWH_MEMCHECK_INIT();

    // Check if running under special tools
    if (cwh_memcheck_is_valgrind())
    {
        printf("Running under Valgrind\n");
    }
    if (cwh_memcheck_is_asan())
    {
        printf("Compiled with AddressSanitizer\n");
    }

    // Run examples
    example_no_leak();
    example_with_leak();
    example_realloc();
    example_statistics();
    example_process_memory();

    // Print current statistics
    printf("\n=== Final Statistics ===\n");
    cwh_memcheck_stats_t stats = cwh_memcheck_get_stats();
    printf("Total allocations: %llu\n", (unsigned long long)stats.total_allocations);
    printf("Total frees: %llu\n", (unsigned long long)stats.total_frees);
    printf("Current leaks: %d\n", cwh_memcheck_has_leaks());

    // Shutdown will print leak report
    printf("\n");
    CWH_MEMCHECK_SHUTDOWN();

    return 0;
}
