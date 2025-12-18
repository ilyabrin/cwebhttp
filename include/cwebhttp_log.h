/**
 * @file cwebhttp_log.h
 * @brief Logging system for cwebhttp
 *
 * Provides leveled logging (DEBUG, INFO, WARN, ERROR) with customizable handlers
 */

#ifndef CWEBHTTP_LOG_H
#define CWEBHTTP_LOG_H

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Log levels
     */
    typedef enum
    {
        CWH_LOG_DEBUG = 0,
        CWH_LOG_INFO = 1,
        CWH_LOG_WARN = 2,
        CWH_LOG_ERROR = 3,
        CWH_LOG_NONE = 4
    } cwh_log_level_t;

    /**
     * @brief Log handler function type
     * @param level Log level
     * @param file Source file name
     * @param line Line number
     * @param func Function name
     * @param message Log message
     * @param user_data User-provided data
     */
    typedef void (*cwh_log_handler_t)(
        cwh_log_level_t level,
        const char *file,
        int line,
        const char *func,
        const char *message,
        void *user_data);

    /**
     * @brief Initialize logging system
     */
    void cwh_log_init(void);

    /**
     * @brief Set minimum log level
     * @param level Minimum level to log
     */
    void cwh_log_set_level(cwh_log_level_t level);

    /**
     * @brief Get current log level
     * @return Current minimum log level
     */
    cwh_log_level_t cwh_log_get_level(void);

    /**
     * @brief Set custom log handler
     * @param handler Handler function
     * @param user_data User data passed to handler
     */
    void cwh_log_set_handler(cwh_log_handler_t handler, void *user_data);

    /**
     * @brief Reset to default log handler (stderr)
     */
    void cwh_log_reset_handler(void);

    /**
     * @brief Set log output file
     * @param filename Output file path
     * @return 0 on success, -1 on error
     */
    int cwh_log_set_file(const char *filename);

    /**
     * @brief Close log file
     */
    void cwh_log_close_file(void);

    /**
     * @brief Enable/disable timestamps
     * @param enabled 1 to enable, 0 to disable
     */
    void cwh_log_set_timestamps(int enabled);

    /**
     * @brief Enable/disable colored output
     * @param enabled 1 to enable, 0 to disable
     */
    void cwh_log_set_colors(int enabled);

    /**
     * @brief Internal logging function
     */
    void cwh_log_internal(
        cwh_log_level_t level,
        const char *file,
        int line,
        const char *func,
        const char *format,
        ...);

    /**
     * @brief Get log level name
     */
    const char *cwh_log_level_name(cwh_log_level_t level);

/* Convenience macros */
#define CWH_LOG_DEBUG(...) cwh_log_internal(CWH_LOG_DEBUG, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define CWH_LOG_INFO(...) cwh_log_internal(CWH_LOG_INFO, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define CWH_LOG_WARN(...) cwh_log_internal(CWH_LOG_WARN, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define CWH_LOG_ERROR(...) cwh_log_internal(CWH_LOG_ERROR, __FILE__, __LINE__, __func__, __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* CWEBHTTP_LOG_H */
