/**
 * @file log.c
 * @brief Logging system implementation
 */

/* Enable POSIX extensions for fileno/isatty */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif

#include "cwebhttp_log.h"
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif

/* Global logging state */
static struct
{
    cwh_log_level_t min_level;
    cwh_log_handler_t handler;
    void *user_data;
    FILE *log_file;
    int timestamps_enabled;
    int colors_enabled;
} g_log_state = {
    .min_level = CWH_LOG_INFO,
    .handler = NULL,
    .user_data = NULL,
    .log_file = NULL,
    .timestamps_enabled = 1,
    .colors_enabled = 1};

/* ANSI color codes */
#ifdef _WIN32
#define COLOR_RESET ""
#define COLOR_DEBUG ""
#define COLOR_INFO ""
#define COLOR_WARN ""
#define COLOR_ERROR ""
#else
#define COLOR_RESET "\033[0m"
#define COLOR_DEBUG "\033[36m" /* Cyan */
#define COLOR_INFO "\033[32m"  /* Green */
#define COLOR_WARN "\033[33m"  /* Yellow */
#define COLOR_ERROR "\033[31m" /* Red */
#endif

/* Check if output supports colors */
static int supports_colors(FILE *stream)
{
#ifdef _WIN32
    DWORD mode;
    intptr_t handle_int = _get_osfhandle(_fileno(stream));
    if (handle_int == -1)
        return 0;
    HANDLE handle = (HANDLE)handle_int;
    if (handle == INVALID_HANDLE_VALUE)
        return 0;
    if (!GetConsoleMode(handle, &mode))
        return 0;
    /* Enable ANSI escape codes on Windows 10+ */
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(handle, mode);
    return 1;
#else
    return isatty(fileno(stream));
#endif
}

void cwh_log_init(void)
{
    g_log_state.min_level = CWH_LOG_INFO;
    g_log_state.handler = NULL;
    g_log_state.user_data = NULL;
    g_log_state.log_file = NULL;
    g_log_state.timestamps_enabled = 1;
    g_log_state.colors_enabled = supports_colors(stderr);
}

void cwh_log_set_level(cwh_log_level_t level)
{
    g_log_state.min_level = level;
}

cwh_log_level_t cwh_log_get_level(void)
{
    return g_log_state.min_level;
}

void cwh_log_set_handler(cwh_log_handler_t handler, void *user_data)
{
    g_log_state.handler = handler;
    g_log_state.user_data = user_data;
}

void cwh_log_reset_handler(void)
{
    g_log_state.handler = NULL;
    g_log_state.user_data = NULL;
}

int cwh_log_set_file(const char *filename)
{
    if (g_log_state.log_file)
    {
        fclose(g_log_state.log_file);
    }

    g_log_state.log_file = fopen(filename, "a");
    if (!g_log_state.log_file)
    {
        return -1;
    }

    g_log_state.colors_enabled = 0; /* Disable colors for file output */
    return 0;
}

void cwh_log_close_file(void)
{
    if (g_log_state.log_file)
    {
        fclose(g_log_state.log_file);
        g_log_state.log_file = NULL;
        g_log_state.colors_enabled = supports_colors(stderr);
    }
}

void cwh_log_set_timestamps(int enabled)
{
    g_log_state.timestamps_enabled = enabled;
}

void cwh_log_set_colors(int enabled)
{
    g_log_state.colors_enabled = enabled;
}

const char *cwh_log_level_name(cwh_log_level_t level)
{
    switch (level)
    {
    case CWH_LOG_DEBUG:
        return "DEBUG";
    case CWH_LOG_INFO:
        return "INFO";
    case CWH_LOG_WARN:
        return "WARN";
    case CWH_LOG_ERROR:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}

static const char *log_level_color(cwh_log_level_t level)
{
    if (!g_log_state.colors_enabled)
        return "";

    switch (level)
    {
    case CWH_LOG_DEBUG:
        return COLOR_DEBUG;
    case CWH_LOG_INFO:
        return COLOR_INFO;
    case CWH_LOG_WARN:
        return COLOR_WARN;
    case CWH_LOG_ERROR:
        return COLOR_ERROR;
    default:
        return COLOR_RESET;
    }
}

static void default_log_handler(
    cwh_log_level_t level,
    const char *file,
    int line,
    const char *func,
    const char *message,
    void *user_data)
{
    (void)user_data;

    FILE *output = g_log_state.log_file ? g_log_state.log_file : stderr;
    const char *color = log_level_color(level);
    const char *reset = g_log_state.colors_enabled ? COLOR_RESET : "";

    /* Timestamp */
    if (g_log_state.timestamps_enabled)
    {
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        char time_buf[32];
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
        fprintf(output, "[%s] ", time_buf);
    }

    /* Level and location */
    fprintf(output, "%s[%-5s]%s %s:%d (%s): %s\n",
            color,
            cwh_log_level_name(level),
            reset,
            file,
            line,
            func,
            message);

    fflush(output);
}

void cwh_log_internal(
    cwh_log_level_t level,
    const char *file,
    int line,
    const char *func,
    const char *format,
    ...)
{
    if (level < g_log_state.min_level)
    {
        return;
    }

    /* Format message */
    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    /* Use custom handler or default */
    if (g_log_state.handler)
    {
        g_log_state.handler(level, file, line, func, message, g_log_state.user_data);
    }
    else
    {
        default_log_handler(level, file, line, func, message, NULL);
    }
}
