// cwebhttp_memcheck.h - Memory leak detection and debugging tools
// Provides malloc/free tracking, leak reports, and integration with Valgrind/ASAN

#ifndef CWEBHTTP_MEMCHECK_H
#define CWEBHTTP_MEMCHECK_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

// ============================================================================
// Configuration
// ============================================================================

// Enable memory tracking (define before including this header)
// #define CWH_MEMCHECK_ENABLED 1

// Maximum number of allocations to track (increase for larger programs)
#ifndef CWH_MEMCHECK_MAX_ALLOCS
#define CWH_MEMCHECK_MAX_ALLOCS 10000
#endif

    // ============================================================================
    // Memory Tracking API
    // ============================================================================

    typedef struct
    {
        void *ptr;          // Allocated pointer
        size_t size;        // Allocation size
        const char *file;   // Source file
        int line;           // Source line
        uint64_t timestamp; // Allocation timestamp (ms)
    } cwh_alloc_info_t;

    typedef struct
    {
        size_t total_allocations;     // Total number of malloc calls
        size_t total_frees;           // Total number of free calls
        size_t current_allocations;   // Currently active allocations
        size_t peak_allocations;      // Peak number of concurrent allocations
        size_t total_bytes_allocated; // Total bytes allocated (cumulative)
        size_t current_bytes;         // Current memory usage
        size_t peak_bytes;            // Peak memory usage
    } cwh_memcheck_stats_t;

    // Initialize memory checker
    void cwh_memcheck_init(void);

    // Cleanup and report leaks
    void cwh_memcheck_shutdown(void);

    // Get memory statistics
    cwh_memcheck_stats_t cwh_memcheck_get_stats(void);

    // Print detailed leak report
    void cwh_memcheck_report(void);

    // Check if there are any leaks (returns number of leaks)
    int cwh_memcheck_has_leaks(void);

    // Reset statistics
    void cwh_memcheck_reset(void);

    // Internal tracking functions (use macros instead)
    void *cwh_memcheck_malloc_internal(size_t size, const char *file, int line);
    void *cwh_memcheck_calloc_internal(size_t nmemb, size_t size, const char *file, int line);
    void *cwh_memcheck_realloc_internal(void *ptr, size_t size, const char *file, int line);
    void cwh_memcheck_free_internal(void *ptr, const char *file, int line);

    // ============================================================================
    // Macro Wrappers
    // ============================================================================

#ifdef CWH_MEMCHECK_ENABLED

// Replace standard memory functions with tracked versions
#define malloc(size) cwh_memcheck_malloc_internal(size, __FILE__, __LINE__)
#define calloc(nmemb, size) cwh_memcheck_calloc_internal(nmemb, size, __FILE__, __LINE__)
#define realloc(ptr, size) cwh_memcheck_realloc_internal(ptr, size, __FILE__, __LINE__)
#define free(ptr) cwh_memcheck_free_internal(ptr, __FILE__, __LINE__)

// Automatic initialization/shutdown helpers
#define CWH_MEMCHECK_INIT() cwh_memcheck_init()
#define CWH_MEMCHECK_SHUTDOWN() cwh_memcheck_shutdown()
#define CWH_MEMCHECK_REPORT() cwh_memcheck_report()

#else

// No-op when disabled
#define CWH_MEMCHECK_INIT() ((void)0)
#define CWH_MEMCHECK_SHUTDOWN() ((void)0)
#define CWH_MEMCHECK_REPORT() ((void)0)

#endif // CWH_MEMCHECK_ENABLED

    // ============================================================================
    // Integration with External Tools
    // ============================================================================

    // Check if running under Valgrind
    int cwh_memcheck_is_valgrind(void);

    // Check if compiled with AddressSanitizer
    int cwh_memcheck_is_asan(void);

    // Get platform-specific memory info
    typedef struct
    {
        size_t rss;         // Resident Set Size (actual RAM usage)
        size_t vsize;       // Virtual memory size
        size_t page_faults; // Number of page faults
    } cwh_process_memory_t;

    int cwh_memcheck_get_process_memory(cwh_process_memory_t *info);

#ifdef __cplusplus
}
#endif

#endif // CWEBHTTP_MEMCHECK_H
