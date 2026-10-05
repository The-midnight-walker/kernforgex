// SPDX-License-Identifier: GPL-2.0
//
// vim: set ts=8 sw=8 noet tw=80 cc=80 fo+=t :

/**
 * @file      debug.h
 * @author    midnight walker
 * @brief     Logging API: console log macros, file logger and log-level table.
 * @version   0.2
 * @date      2026-09-30
 *
 * @note      Revision 0.3
 *
 * @copyright GNU General Public License v2.0
 */

#ifndef INCLUDE_GENERIC_DEBUG_H
#define INCLUDE_GENERIC_DEBUG_H

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

/**
 * @brief Convenience mask with every log level enabled.
 */
#ifndef LOG_MASK_ALL
#define LOG_MASK_ALL                                                           \
    (LOG_LEVEL_FATAL | LOG_LEVEL_WARN | LOG_LEVEL_ERROR | LOG_LEVEL_INFO |     \
     LOG_LEVEL_DEBUG)
#endif
/** @} */

/**
 * @name ANSI color escape sequences
 * @{
 */
#define RED "\033[0;31m"
#define GREEN "\033[0;32m"
#define YELLOW "\033[0;33m"
#define BLUE "\033[0;34m"
#define MAGENTA "\033[0;35m"
#define CYAN "\033[0;36m"
#define RESET "\033[0m"
/** @} */

enum log_stream_type {
    STREAM_STDOUT, /**< Written to stdout */
    STREAM_STDERR  /**< Written to stderr */
};

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
#ifndef LOG_MSG_BUF_SIZE
#define LOG_MSG_BUF_SIZE 1024 /**< Max formatted user message size */
#endif

#ifndef LOG_HEADER_EXTRA_SIZE
#define LOG_HEADER_EXTRA_SIZE 32 /**< Room for timestamp and decoration */
#endif

#ifndef LOG_LINE_BUF_SIZE
#define LOG_LINE_BUF_SIZE (LOG_MSG_BUF_SIZE + LOG_HEADER_EXTRA_SIZE)
#endif
/** @} */

/**
 * @name   File logger timestamp configuration
 * @brief  Configures optional timestamping string format for the file logger
 * when LOG_TIMESTAMP is defined by the build system.
 * @details Allows overriding the default strftime() format string
 *          by defining LOG_TIMESTAMP_FMT.
 * @{
 */
#ifdef LOG_TIMESTAMP

#ifndef LOG_TIMESTAMP_FMT
#define LOG_TIMESTAMP_FMT "%Y-%m-%d %H:%M:%S"
#endif

#endif /* LOG_TIMESTAMP */
/** @} */

/**
 * @name Per-level colors
 * @{
 */
#ifndef LOG_LEVEL_FATAL_COLOR
#define LOG_LEVEL_FATAL_COLOR RED
#endif

#ifndef LOG_LEVEL_WARN_COLOR
#define LOG_LEVEL_WARN_COLOR RED
#endif

#ifndef LOG_LEVEL_ERROR_COLOR
#define LOG_LEVEL_ERROR_COLOR RED
#endif

#ifndef LOG_LEVEL_INFO_COLOR
#define LOG_LEVEL_INFO_COLOR GREEN
#endif

#ifndef LOG_LEVEL_DEBUG_COLOR
#define LOG_LEVEL_DEBUG_COLOR CYAN
#endif
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
#define pr_cyan(stream, fmt, ...) PR_COLOR(stream, CYAN, fmt, ##__VA_ARGS__)
#define pr_red(stream, fmt, ...) PR_COLOR(stream, RED, fmt, ##__VA_ARGS__)
#define pr_green(stream, fmt, ...) PR_COLOR(stream, GREEN, fmt, ##__VA_ARGS__)
#define pr_yellow(stream, fmt, ...) PR_COLOR(stream, YELLOW, fmt, ##__VA_ARGS__)
#define pr_blue(stream, fmt, ...) PR_COLOR(stream, BLUE, fmt, ##__VA_ARGS__)
#define pr_magenta(stream, fmt, ...)                                           \
    PR_COLOR(stream, MAGENTA, fmt, ##__VA_ARGS__)
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
 * @brief Initializes the logger.
 *
 * @note **CONCURRENCY && THREAD-SAFETY:** Thread-safe; protected by internal
 * synchronization ensuring only the first initialization call succeeds.
 *
 * @param[in] cli_log_mask      Initial level mask for console output.
 * @param[in] log_file_pathname Log file path, or NULL/"" to disable the file.
 * @param[in] flags             Extra open(2) flags.
 * @param[in] perms             File permissions used when the file is created.
 * @param[in] file_log_mask     Initial level mask for file output.
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
 * @brief Returns the console log mask.
 *
 * Before init_logging_ctx() is called, returns LOG_MASK_ALL (every level
 * enabled) rather than 0, so pr_*() prints to the console out of the box
 * even in programs that never set up the file logger.
 *
 * @note **CONCURRENCY && THREAD-SAFETY:** Safe to call concurrently; reads an
 * atomic value.
 */
unsigned int get_cli_log_mask(void);

/**
 * @brief Returns the file log mask, or 0 if the logger is not initialized
 * (there is then no log file open to write to regardless of the mask).
 *
 * @note **CONCURRENCY && THREAD-SAFETY:** Safe to call concurrently; reads an
 * atomic value.
 */
unsigned int get_file_log_mask(void);

/**
 * @brief Atomically replaces the console log mask.
 *
 * @param[in] mask New bitmask of LOG_LEVEL_* values.
 *
 * @return LOG_SUCCESS, or LOG_ENOINIT if the logger is not initialized.
 *
 * @note **CONCURRENCY && THREAD-SAFETY:** Thread-safe; performs an atomic
 * store.
 */
int set_cli_log_mask(const unsigned int mask);

/**
 * @brief Atomically replaces the file log mask.
 *
 * @param[in] mask New bitmask of LOG_LEVEL_* values.
 *
 * @return LOG_SUCCESS, or LOG_ENOINIT if the logger is not initialized.
 *
 * @note **CONCURRENCY && THREAD-SAFETY:** Thread-safe; performs an atomic
 * store.
 */
int set_file_log_mask(const unsigned int mask);

/**
 * @brief Writes one printf-formatted line to the log file.
 *
 * The message is truncated to LOG_MSG_BUF_SIZE - 1 bytes and prefixed with a
 * timestamp when LOG_TIMESTAMP is defined. A newline is always appended.
 *
 * @param[in] fmt printf-style format string. Must not be NULL.
 *
 * @return LOG_SUCCESS, LOG_EINVALARG (NULL fmt), LOG_ENOINIT (logger not
 * initialized or no log file open), or LOG_ESYS (errno is set).
 *
 * @note **CONCURRENCY && THREAD-SAFETY:** Thread-safe; internal operations and
 * file writes are fully serialized under `log_ctx.mutex`.
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
 * @brief Selects what the console macros print, based on the project-wide
 * DEBUG macro (the same one modules.c already checks for its own extra
 * diagnostics).
 *
 * Define DEBUG (-DDEBUG) to enable DEBUG-level console output and the
 * [ func:file:line ] suffix on every console line. Without it, pr_debug()
 * is compiled out at the console (see LOG_CLI()) and the suffix is omitted.
 * @{
 */
#ifdef DEBUG

#define LOG_META_FMT " [ %s:%s:%d ]\n"
#define LOG_META_FMT_COLOR COLOR_META_FMT " [ %s:%s:%d ]\n" RESET
#define LOG_META_ARGS                                                          \
    , __func__, (strncmp(__FILE__, "./", 2) == 0 ? (__FILE__ + 2) : __FILE__), \
        __LINE__

#else

#define LOG_META_FMT "\n"
#define LOG_META_FMT_COLOR "\n"
#define LOG_META_ARGS

#endif /* DEBUG */
/** @} */

/**
 * @brief Writes a formatted log line to the console (stdout or stderr).
 *
 * Output layout: `[ LEVEL ] <prfx_fmt><message>`, followed by
 * ` [ func:file:line ]` when DEBUG is defined.
 *
 * When LOG_TTY_COLOR is defined, the log line is colorized using the ANSI
 * escape sequence associated with the log level (via the LOG_LEVELS table)
 * and a reset code is appended at the end. When LOG_TTY_COLOR is not
 * defined, plain text is written without any color codes.
 *
 * DEBUG-level messages are dropped entirely unless DEBUG is defined; their
 * arguments are still compiled (and their format checked) but never
 * evaluated at run time.
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
 * @brief Writes a formatted log line to the persistent log file.
 *
 * Output layout: `[ LEVEL ] <prfx_fmt><message>`, followed by
 * ` [ func:file:line ]` when DEBUG is defined.
 * DEBUG-level messages are dropped entirely unless DEBUG is defined.
 *
 * @param level_index Constant from enum log_level_index.
 * @param fmt         printf format string literal.
 */
#define LOG_FILE(level_index, fmt, ...)                                        \
    do {                                                                       \
                                                                               \
        log_write(                                                             \
            "[ %s ] " prfx_fmt fmt LOG_META_FMT,                               \
            LOG_LEVELS[level_index].status,                                    \
            ##__VA_ARGS__ LOG_META_ARGS);                                      \
                                                                               \
    } while (0)

/**
 * @brief Runtime-filtered core of the pr_*() macros.
 *
 * @param idx Constant from enum log_level_index.
 * @param bit Matching LOG_LEVEL_* bitmask value.
 * @param fmt printf format string literal.
 */
#define PR_LEVEL(idx, bit, fmt, ...)                                           \
    do {                                                                       \
        if (get_cli_log_mask() & (bit))                                        \
            LOG_CLI(idx, fmt, ##__VA_ARGS__);                                  \
        if (get_file_log_mask() & (bit))                                       \
            LOG_FILE(idx, fmt, ##__VA_ARGS__);                                 \
    } while (0)

/**
 * @name Console/file log macros
 * @brief Log at the given level, to both the console and the log file.
 * @{
 */
#define pr_fatal(fmt, ...)                                                     \
    PR_LEVEL(IDX_FATAL, LOG_LEVEL_FATAL, fmt, ##__VA_ARGS__)
#define pr_warn(fmt, ...) PR_LEVEL(IDX_WARN, LOG_LEVEL_WARN, fmt, ##__VA_ARGS__)
#define pr_error(fmt, ...)                                                     \
    PR_LEVEL(IDX_ERROR, LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#define pr_info(fmt, ...) PR_LEVEL(IDX_INFO, LOG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#define pr_debug(fmt, ...)                                                     \
    PR_LEVEL(IDX_DEBUG, LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
/** @} */

#endif /* INCLUDE_GENERIC_DEBUG_H */