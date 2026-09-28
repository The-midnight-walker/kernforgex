// SPDX-License-Identifier: GPL-2.0
//
// vim: set ts=8 sw=8 noet tw=80 cc=80 fo+=t :

/**
 * @file      debug.h
 * @author    midnight walker
 * @brief     Logging API: console log macros, file logger and log-level table.
 * @version   0.2
 * @date      2026-09-28
 *
 * @copyright GNU General Public License v2.0
 */

#ifndef INCLUDE_DEBUG_H
#define INCLUDE_DEBUG_H

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

/**
 * @brief Per-translation-unit message prefix, inserted after the level tag.
 *
 * Define it before including this header.
 * Must be a string literal. Defaults to an empty string.
 */
#ifndef prfx_fmt
#define prfx_fmt ""
#endif

/**
 * @enum log_status
 * @brief Return codes of the file logger API.
 */
enum log_status {
    LOG_SUCCESS = 0,       /**< Operation completed successfully */
    LOG_ENOINIT = -1,      /**< Logger not initialized (or no log file open) */
    LOG_EALREADYINIT = -2, /**< init_logging_ctx() was already called */
    LOG_EINVALARG = -3,    /**< Invalid argument (e.g. NULL format string) */
    LOG_ESYS = -4          /**< System error, details are available in errno */
};

/**
 * @name Log level bitmasks
 * @brief Bit values combined into the CLI and file log masks.
 * @{
 */
#define LOG_LEVEL_FATAL 0x01
#define LOG_LEVEL_WARN 0x02
#define LOG_LEVEL_ERROR 0x04
#define LOG_LEVEL_INFO 0x08
#define LOG_LEVEL_DEBUG 0x10
/** @} */

/**
 * @name ANSI color escape sequences
 * @{
 */
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN "\033[36m"
#define RESET "\033[0m"
/** @} */

enum log_stream_type {
    STREAM_STDOUT, /**< Written to stdout */
    STREAM_STDERR  /**< Written to stderr */
};

/**
 * @brief Default value stored in log_level::display_tag.
 *
 * Informational only: no code in the logger currently consumes this field.
 */
#define DISPLAY_TAG 9

/**
 * @enum log_level_index
 * @brief Indexes into the LOG_LEVELS table, one per log level.
 *
 * The order must match the initializers of LOG_LEVELS in debug.c.
 */
enum log_level_index {
    IDX_FATAL = 0, /**< Fatal errors */
    IDX_WARN,      /**< Warnings */
    IDX_ERROR,     /**< Recoverable errors */
    IDX_INFO,      /**< Informational messages */
    IDX_DEBUG,     /**< Debug traces */
    IDX_COUNT      /**< Number of levels (keep last, not a valid level) */
};

/**
 * @struct log_level
 * @brief Static description of a log level.
 */
struct log_level {
    const char *status;               /**< Level name (e.g. "ERROR") */
    unsigned int bit_val;             /**< Bitmask value (LOG_LEVEL_*) */
    int display_tag;                  /**< Display tag (see DISPLAY_TAG) */
    const char *tty_color;            /**< ANSI color used on a TTY */
    enum log_stream_type stream_type; /**< Output stream of this level */
};

/**
 * @brief Table of all log levels, indexed by enum log_level_index.
 */
extern const struct log_level LOG_LEVELS[IDX_COUNT];

/**
 * @brief Number of entries in LOG_LEVELS.
 */
#define LOG_LEVELS_NR ((size_t)IDX_COUNT)

/**
 * @name Default console format colors
 * @brief Overridable before including this header.
 * @{
 */
#ifndef COLOR_PRFX_FMT
#define COLOR_PRFX_FMT YELLOW /**< Color of the prfx_fmt prefix */
#endif

#ifndef COLOR_META_FMT
#define COLOR_META_FMT GREEN /**< Color of the [ func:file:line ] suffix */
#endif
/** @} */

/**
 * @name Buffer sizes
 * @{
 */
#define LOG_MSG_BUF_SIZE 1024    /**< Max formatted user message size */
#define LOG_HEADER_EXTRA_SIZE 32 /**< Room for timestamp and decoration */
#define LOG_LINE_BUF_SIZE (LOG_MSG_BUF_SIZE + LOG_HEADER_EXTRA_SIZE)
/** @} */

/**
 * @name File logger timestamp
 * @brief Define LOG_TIMESTAMP (and optionally LOG_TIMESTAMP_FMT, a strftime()
 * format) from the build system to override the defaults below.
 * @{
 */
#ifndef LOG_TIMESTAMP
#define LOG_TIMESTAMP
#endif

#ifndef LOG_TIMESTAMP_FMT
#define LOG_TIMESTAMP_FMT "%Y-%m-%d %H:%M:%S"
#endif
/** @} */

/**
 * @name Per-level colors
 * @{
 */
#define LOG_LEVEL_FATAL_COLOR RED
#define LOG_LEVEL_WARN_COLOR YELLOW
#define LOG_LEVEL_ERROR_COLOR RED
#define LOG_LEVEL_INFO_COLOR BLUE
#define LOG_LEVEL_DEBUG_COLOR CYAN
/** @} */

/**
 * @brief Prints a printf-style message wrapped in an ANSI color.
 *
 * @param stream Destination FILE stream.
 * @param color  ANSI color literal (e.g. RED).
 * @param fmt    printf format string literal.
 */
#define PR_COLOR(stream, color, fmt, ...)                                      \
    fprintf((stream), color fmt RESET, ##__VA_ARGS__)

/**
 * @brief Plain fprintf() wrapper, kept for symmetry with the pr_<color>()
 * helpers.
 */
#define pr(stream, fmt, ...) fprintf((stream), fmt, ##__VA_ARGS__)

/**
 * @name Colored print helpers
 * @brief Same signature as fprintf(): (stream, fmt, ...).
 * @{
 */
#define pr_red(stream, fmt, ...) PR_COLOR(stream, RED, fmt, ##__VA_ARGS__)
#define pr_green(stream, fmt, ...) PR_COLOR(stream, GREEN, fmt, ##__VA_ARGS__)
#define pr_yellow(stream, fmt, ...) PR_COLOR(stream, YELLOW, fmt, ##__VA_ARGS__)
#define pr_blue(stream, fmt, ...) PR_COLOR(stream, BLUE, fmt, ##__VA_ARGS__)
#define pr_magenta(stream, fmt, ...)                                           \
    PR_COLOR(stream, MAGENTA, fmt, ##__VA_ARGS__)
#define pr_cyan(stream, fmt, ...) PR_COLOR(stream, CYAN, fmt, ##__VA_ARGS__)
/** @} */

/**
 * @struct log_ctx
 * @brief Internal state of the file logger.
 *
 * Exposed for the implementation in debug.c only; do not access directly.
 */
struct log_ctx {
    atomic_uint cli_log_mask;  /**< Level mask for console output */
    atomic_uint file_log_mask; /**< Level mask for file output */
    char *log_file_pathname;   /**< Heap copy of the log file path, or NULL */
    int log_fd;                /**< Log file descriptor, or -1 if none */
    atomic_bool is_init;       /**< True once init_logging_ctx() succeeded */
    pthread_mutex_t mutex;     /**< Protects log_fd, pathname and file writes */
};

/**
 * @brief Initializes the logger. Thread-safe; only the first call succeeds.
 *
 * @param[in] cli_log_mask     Initial level mask for console output.
 * @param[in] log_file_pathname Log file path, or NULL/"" to disable the file.
 * @param[in] flags            Extra open(2) flags (e.g. O_APPEND, O_TRUNC),
 * OR-ed with O_WRONLY | O_CREAT | O_CLOEXEC.
 * @param[in] perms            File permissions used when the file is created.
 * @param[in] file_log_mask    Initial level mask for file output.
 *
 * @return LOG_SUCCESS on success, LOG_EALREADYINIT if already initialized, or
 * LOG_ESYS on allocation/open failure (errno is set).
 */
int init_logging_ctx(
    unsigned int cli_log_mask,
    const char *log_file_pathname,
    int flags,
    mode_t perms,
    unsigned int file_log_mask);

/**
 * @brief Returns the console log mask, or 0 if the logger is not initialized.
 */
unsigned int get_cli_log_mask(void);

/**
 * @brief Returns the file log mask, or 0 if the logger is not initialized.
 */
unsigned int get_file_log_mask(void);

/**
 * @brief Atomically replaces the console log mask.
 *
 * @param[in] mask New bitmask of LOG_LEVEL_* values.
 *
 * @return LOG_SUCCESS, or LOG_ENOINIT if the logger is not initialized.
 */
int set_cli_log_mask(const unsigned int mask);

/**
 * @brief Atomically replaces the file log mask.
 *
 * @param[in] mask New bitmask of LOG_LEVEL_* values.
 *
 * @return LOG_SUCCESS, or LOG_ENOINIT if the logger is not initialized.
 */
int set_file_log_mask(const unsigned int mask);

/**
 * @brief Writes one printf-formatted line to the log file. Thread-safe.
 *
 * The message is truncated to LOG_MSG_BUF_SIZE - 1 bytes and prefixed with a
 * timestamp when LOG_TIMESTAMP is defined. A newline is always appended.
 *
 * @param[in] fmt printf-style format string. Must not be NULL.
 *
 * @return LOG_SUCCESS, LOG_EINVALARG (NULL fmt), LOG_ENOINIT (logger not
 * initialized or no log file open), or LOG_ESYS (errno is set).
 */
int log_write(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/**
 * @brief Prints a log file to the console, colorizing each line by level.
 *
 * A line is attributed to the first LOG_LEVELS entry whose name it contains
 * and is sent to that level's stream. Colors are emitted only when
 * LOG_TTY_COLOR is defined and stdout is a terminal.
 *
 * @param[in] filepath Path of the log file to read.
 *
 * @return 0 on success, -1 on error (invalid path or fopen() failure).
 */
int print_colored_log(const char *filepath);

/**
 * @name Debug-mode configuration
 * @brief Selects what the console macros print, based on LOG_DEBUG_MODE.
 *
 * Define LOG_DEBUG_MODE (e.g. -DLOG_DEBUG_MODE) to enable debug output and
 * source-location metadata. Without it, pr_debug() prints nothing.
 * @{
 */
#ifdef LOG_DEBUG_MODE

#define LOG_DEBUG_ENABLED 1
#define LOG_META_FMT " [ %s:%s:%d ]\n"
#define LOG_META_FMT_COLOR COLOR_META_FMT " [ %s:%s:%d ]\n" RESET
#define LOG_META_ARGS                                                          \
    , __func__, (strncmp(__FILE__, "./", 2) == 0 ? (__FILE__ + 2) : __FILE__), \
        __LINE__

#else

#define LOG_DEBUG_ENABLED 0
#define LOG_META_FMT "\n"
#define LOG_META_FMT_COLOR "\n"
#define LOG_META_ARGS

#endif /* LOG_DEBUG_MODE */
/** @} */

/**
 * @brief Writes a formatted log line to the console (stdout or stderr).
 *
 * Output layout: `[ LEVEL ] <prfx_fmt><message>`, followed by
 * ` [ func:file:line ]` when LOG_DEBUG_MODE is defined.
 *
 * When LOG_TTY_COLOR is defined, the log line is colorized using the
 * ANSI escape sequences associated with the log level (via the LOG_LEVELS
 * table) and reset codes are appended at the end. When LOG_TTY_COLOR is not
 * defined, plain text is written without any color codes.
 *
 * DEBUG-level messages are dropped entirely unless
 * LOG_DEBUG_MODE is defined; their arguments are still compiled (and their
 * format checked) but never evaluated at run time.
 *
 * The level name and color come from the LOG_LEVELS table and are passed as
 * "%s" arguments, since they are runtime values and cannot be spliced into
 * the format string literal.
 *
 * @param level_index Constant from enum log_level_index.
 * @param fmt         printf format string literal.
 *
 * @note level_index is evaluated several times; pass a constant.
 */
#ifdef LOG_TTY_COLOR

#define LOG_CLI(level_index, fmt, ...)                                         \
    do {                                                                       \
        if (!LOG_DEBUG_ENABLED && (level_index) == IDX_DEBUG)                  \
            break;                                                             \
                                                                               \
        FILE *log_cli_stream =                                                 \
            (LOG_LEVELS[level_index].stream_type == STREAM_STDERR) ? stderr    \
                                                                   : stdout;   \
                                                                               \
        fprintf(                                                               \
            log_cli_stream,                                                    \
            "%s[ %s ] " RESET COLOR_PRFX_FMT prfx_fmt RESET fmt                \
                LOG_META_FMT_COLOR,                                            \
            LOG_LEVELS[level_index].tty_color,                                 \
            LOG_LEVELS[level_index].status,                                    \
            ##__VA_ARGS__ LOG_META_ARGS);                                      \
        fflush(log_cli_stream);                                                \
    } while (0)

#else

#define LOG_CLI(level_index, fmt, ...)                                         \
    do {                                                                       \
        if (!LOG_DEBUG_ENABLED && (level_index) == IDX_DEBUG)                  \
            break;                                                             \
                                                                               \
        FILE *log_cli_stream =                                                 \
            (LOG_LEVELS[level_index].stream_type == STREAM_STDERR) ? stderr    \
                                                                   : stdout;   \
                                                                               \
        fprintf(                                                               \
            log_cli_stream,                                                    \
            "[ %s ] " prfx_fmt fmt LOG_META_FMT,                               \
            LOG_LEVELS[level_index].status,                                    \
            ##__VA_ARGS__ LOG_META_ARGS);                                      \
        fflush(log_cli_stream);                                                \
    } while (0)

#endif /* LOG_TTY_COLOR */

/**
 * @name Console log macros
 * @brief Log at the given level. Same usage as printf(): (fmt, ...).
 * @{
 */
#define pr_fatal(fmt, ...) LOG_CLI(IDX_FATAL, fmt, ##__VA_ARGS__)
#define pr_warn(fmt, ...) LOG_CLI(IDX_WARN, fmt, ##__VA_ARGS__)
#define pr_error(fmt, ...) LOG_CLI(IDX_ERROR, fmt, ##__VA_ARGS__)
#define pr_info(fmt, ...) LOG_CLI(IDX_INFO, fmt, ##__VA_ARGS__)
#define pr_debug(fmt, ...) LOG_CLI(IDX_DEBUG, fmt, ##__VA_ARGS__)
/** @} */

#endif /* INCLUDE_DEBUG_H */