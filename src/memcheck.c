// memcheck.c - Memory leak detection implementation

#include "cwebhttp_memcheck.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#endif

// ============================================================================
// Internal State
// ============================================================================

static cwh_alloc_info_t g_alloc_table[CWH_MEMCHECK_MAX_ALLOCS];
static int g_alloc_count = 0;
static cwh_memcheck_stats_t g_stats = {0};
static int g_initialized = 0;

// ============================================================================
// Helper Functions
// ============================================================================

static uint64_t get_timestamp_ms(void)
{
#ifdef _WIN32
    return GetTickCount64();
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
}

static int find_alloc_slot(void *ptr)
{
    for (int i = 0; i < g_alloc_count; i++)
    {
        if (g_alloc_table[i].ptr == ptr)
        {
            return i;
        }
    }
    return -1;
}

static void add_allocation(void *ptr, size_t size, const char *file, int line)
{
    if (g_alloc_count >= CWH_MEMCHECK_MAX_ALLOCS)
    {
        fprintf(stderr, "[MEMCHECK] WARNING: Allocation table full! Increase CWH_MEMCHECK_MAX_ALLOCS\n");
        return;
    }

    g_alloc_table[g_alloc_count].ptr = ptr;
    g_alloc_table[g_alloc_count].size = size;
    g_alloc_table[g_alloc_count].file = file;
    g_alloc_table[g_alloc_count].line = line;
    g_alloc_table[g_alloc_count].timestamp = get_timestamp_ms();
    g_alloc_count++;

    // Update statistics
    g_stats.total_allocations++;
    g_stats.current_allocations++;
    g_stats.total_bytes_allocated += size;
    g_stats.current_bytes += size;

    if (g_stats.current_allocations > g_stats.peak_allocations)
    {
        g_stats.peak_allocations = g_stats.current_allocations;
    }

    if (g_stats.current_bytes > g_stats.peak_bytes)
    {
        g_stats.peak_bytes = g_stats.current_bytes;
    }
}

static void remove_allocation(void *ptr)
{
    int slot = find_alloc_slot(ptr);
    if (slot < 0)
    {
        fprintf(stderr, "[MEMCHECK] WARNING: Free of untracked pointer %p\n", ptr);
        return;
    }

    // Update statistics
    g_stats.total_frees++;
    g_stats.current_allocations--;
    g_stats.current_bytes -= g_alloc_table[slot].size;

    // Remove from table (swap with last element)
    g_alloc_count--;
    if (slot != g_alloc_count)
    {
        g_alloc_table[slot] = g_alloc_table[g_alloc_count];
    }
}

// ============================================================================
// Public API Implementation
// ============================================================================

void cwh_memcheck_init(void)
{
    if (g_initialized)
    {
        return;
    }

    memset(&g_stats, 0, sizeof(g_stats));
    g_alloc_count = 0;
    g_initialized = 1;

    printf("[MEMCHECK] Memory leak detection initialized\n");
    printf("[MEMCHECK] Max tracked allocations: %d\n", CWH_MEMCHECK_MAX_ALLOCS);
}

void cwh_memcheck_shutdown(void)
{
    if (!g_initialized)
    {
        return;
    }

    printf("\n[MEMCHECK] ========================================\n");
    printf("[MEMCHECK] Memory Leak Detection Report\n");
    printf("[MEMCHECK] ========================================\n");

    if (g_alloc_count > 0)
    {
        printf("[MEMCHECK] WARNING: %d memory leak(s) detected!\n\n", g_alloc_count);
        cwh_memcheck_report();
    }
    else
    {
        printf("[MEMCHECK] SUCCESS: No memory leaks detected!\n");
    }

    printf("\n[MEMCHECK] Statistics:\n");
    printf("[MEMCHECK]   Total allocations: %zu\n", g_stats.total_allocations);
    printf("[MEMCHECK]   Total frees:       %zu\n", g_stats.total_frees);
    printf("[MEMCHECK]   Peak allocations:  %zu\n", g_stats.peak_allocations);
    printf("[MEMCHECK]   Peak memory usage: %zu bytes (%.2f KB)\n",
           g_stats.peak_bytes, g_stats.peak_bytes / 1024.0);
    printf("[MEMCHECK] ========================================\n");

    g_initialized = 0;
}

cwh_memcheck_stats_t cwh_memcheck_get_stats(void)
{
    return g_stats;
}

void cwh_memcheck_report(void)
{
    if (g_alloc_count == 0)
    {
        printf("[MEMCHECK] No leaks to report\n");
        return;
    }

    printf("[MEMCHECK] Leaked allocations:\n");
    for (int i = 0; i < g_alloc_count; i++)
    {
        printf("[MEMCHECK]   [%d] %zu bytes at %p\n",
               i + 1, g_alloc_table[i].size, g_alloc_table[i].ptr);
        printf("[MEMCHECK]       Allocated at %s:%d\n",
               g_alloc_table[i].file, g_alloc_table[i].line);
        printf("[MEMCHECK]       Age: %llu ms\n",
               (unsigned long long)(get_timestamp_ms() - g_alloc_table[i].timestamp));
    }
}

int cwh_memcheck_has_leaks(void)
{
    return g_alloc_count;
}

void cwh_memcheck_reset(void)
{
    memset(&g_stats, 0, sizeof(g_stats));
    g_alloc_count = 0;
}

// ============================================================================
// Tracked Memory Functions
// ============================================================================

#undef malloc
#undef calloc
#undef realloc
#undef free

void *cwh_memcheck_malloc_internal(size_t size, const char *file, int line)
{
    void *ptr = malloc(size);
    if (ptr && g_initialized)
    {
        add_allocation(ptr, size, file, line);
    }
    return ptr;
}

void *cwh_memcheck_calloc_internal(size_t nmemb, size_t size, const char *file, int line)
{
    void *ptr = calloc(nmemb, size);
    if (ptr && g_initialized)
    {
        add_allocation(ptr, nmemb * size, file, line);
    }
    return ptr;
}

void *cwh_memcheck_realloc_internal(void *ptr, size_t size, const char *file, int line)
{
    if (ptr && g_initialized)
    {
        remove_allocation(ptr);
    }

    void *new_ptr = realloc(ptr, size);
    if (new_ptr && g_initialized)
    {
        add_allocation(new_ptr, size, file, line);
    }

    return new_ptr;
}

void cwh_memcheck_free_internal(void *ptr, const char *file, int line)
{
    (void)file;
    (void)line;

    if (!ptr)
    {
        return;
    }

    if (g_initialized)
    {
        remove_allocation(ptr);
    }

    free(ptr);
}

// ============================================================================
// External Tool Integration
// ============================================================================

int cwh_memcheck_is_valgrind(void)
{
#ifdef __linux__
    // Check if RUNNING_ON_VALGRIND is defined (requires valgrind headers)
    // For now, check environment variable
    return getenv("VALGRIND_OPTS") != NULL;
#else
    return 0;
#endif
}

int cwh_memcheck_is_asan(void)
{
#ifdef __SANITIZE_ADDRESS__
    return 1;
#else
    return 0;
#endif
}

int cwh_memcheck_get_process_memory(cwh_process_memory_t *info)
{
    if (!info)
    {
        return -1;
    }

    memset(info, 0, sizeof(*info));

#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
    {
        info->rss = pmc.WorkingSetSize;
        info->vsize = pmc.PagefileUsage;
        info->page_faults = pmc.PageFaultCount;
        return 0;
    }
    return -1;
#elif defined(__linux__)
    FILE *f = fopen("/proc/self/statm", "r");
    if (!f)
    {
        return -1;
    }

    unsigned long vsize, rss;
    if (fscanf(f, "%lu %lu", &vsize, &rss) == 2)
    {
        long page_size = sysconf(_SC_PAGESIZE);
        info->vsize = vsize * page_size;
        info->rss = rss * page_size;

        // Get page faults from /proc/self/stat
        fclose(f);
        f = fopen("/proc/self/stat", "r");
        if (f)
        {
            char buf[1024];
            if (fgets(buf, sizeof(buf), f))
            {
                unsigned long minflt, majflt;
                if (sscanf(buf, "%*d %*s %*c %*d %*d %*d %*d %*d %*u %lu %*u %lu",
                           &minflt, &majflt) == 2)
                {
                    info->page_faults = minflt + majflt;
                }
            }
            fclose(f);
        }
        return 0;
    }

    fclose(f);
    return -1;
#elif defined(__APPLE__)
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0)
    {
        info->rss = usage.ru_maxrss;
        info->page_faults = usage.ru_minflt + usage.ru_majflt;
        return 0;
    }
    return -1;
#else
    return -1; // Unsupported platform
#endif
}
