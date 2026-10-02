// SPDX-License-Identifier: GPL-2.0
//
// vim: set ts=8 sw=8 noet tw=80 cc=80 fo+=t :

/**
 * @file      debug.c
 * @author    midnight walker
 * @brief     Logging implementation: file logger and log viewer.
 * @version   0.3
 * @date      2026-09-28
 *
 * @note      Revision 0.3
 *
 * @copyright GNU General Public License v2.0
 */

#define prfx_fmt "debug: "

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "debug.h"

/**
 * @brief Global logger state.
 *
 * Protected by ctx.mutex for the file descriptor, the pathname and file
 * writes; the masks and the init flag are atomics and may be read lock-free.
 * Zero-initialized members (masks) start at 0.
 */
static struct log_ctx ctx = {
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .is_init = false,
    .log_fd = -1,
    .log_file_pathname = NULL};

/**
 * @brief Table of log levels, indexed by enum log_level_index.
 *
 * Designated initializers tie each entry to its index, so reordering the
 * enum can no longer silently mismatch the table.
 */
const struct log_level LOG_LEVELS[IDX_COUNT] = {
    [IDX_FATAL] =
        {"FATAL", LOG_LEVEL_FATAL, LOG_LEVEL_FATAL_COLOR, STREAM_STDERR},
    [IDX_WARN] = {"WARN ", LOG_LEVEL_WARN, LOG_LEVEL_WARN_COLOR, STREAM_STDERR},
    [IDX_ERROR] =
        {"ERROR", LOG_LEVEL_ERROR, LOG_LEVEL_ERROR_COLOR, STREAM_STDERR},
    [IDX_INFO] = {"INFO ", LOG_LEVEL_INFO, LOG_LEVEL_INFO_COLOR, STREAM_STDOUT},
    [IDX_DEBUG] =
        {"DEBUG", LOG_LEVEL_DEBUG, LOG_LEVEL_DEBUG_COLOR, STREAM_STDOUT},
};

/**
 * @brief Writes a whole buffer to a file descriptor, retrying as needed.
 *
 * @param[in] fd    Destination file descriptor.
 * @param[in] buf   Data to write.
 * @param[in] count Number of bytes to write.
 *
 * @return true if all bytes were written, false on a real I/O error (errno
 * is preserved for the caller).
 */
static bool write_all(int fd, const char *buf, size_t count)
{
    size_t total_written = 0;

    while (total_written < count) {
        ssize_t written = write(fd, buf + total_written, count - total_written);

        if (written < 0) {
            if (errno == EINTR)
                continue; /* Interrupted by a signal, retry */

            return false; /* Real I/O error (errno is preserved) */
        }

        if (written == 0) {
            /* Zero bytes written: cannot make progress, avoid busy-loop */
            errno = EIO;
            return false;
        }

        total_written += (size_t)written;
    }

    fsync(fd);

    return true;
}

int init_logging_ctx(
    unsigned int cli_log_mask,
    const char *log_file_pathname,
    int flags,
    mode_t perms,
    unsigned int file_log_mask)
{
    pthread_mutex_lock(&ctx.mutex);

    /* Reject a second initialization */
    if (atomic_load(&ctx.is_init)) {
        pthread_mutex_unlock(&ctx.mutex);
        errno = EALREADY;
        return LOG_EALREADYINIT;
    }

    /* Initialize the atomic masks */
    atomic_store(&ctx.cli_log_mask, cli_log_mask);
    atomic_store(&ctx.file_log_mask, file_log_mask);

    /* Open the log file if a path was given */
    if (log_file_pathname != NULL && log_file_pathname[0] != '\0') {

        ctx.log_file_pathname = strdup(log_file_pathname);
        if (ctx.log_file_pathname == NULL) {
            pthread_mutex_unlock(&ctx.mutex);
            errno = ENOMEM;
            return LOG_ESYS;
        }

        int open_flags = O_WRONLY | O_CREAT | O_CLOEXEC | flags;

        ctx.log_fd = open(ctx.log_file_pathname, open_flags, perms);

        if (ctx.log_fd == -1) {
            int saved_errno = errno;

            free(ctx.log_file_pathname);
            ctx.log_file_pathname = NULL;

            pthread_mutex_unlock(&ctx.mutex);
            errno = saved_errno;
            return LOG_ESYS;
        }
    } else {
        ctx.log_fd = -1;
        ctx.log_file_pathname = NULL;
    }

    /* Publish the initialized state */
    atomic_store(&ctx.is_init, true);

    pthread_mutex_unlock(&ctx.mutex);

    return LOG_SUCCESS;
}

unsigned int get_cli_log_mask(void)
{
    if (!atomic_load(&ctx.is_init))
        return LOG_MASK_ALL;

    return atomic_load(&ctx.cli_log_mask);
}

unsigned int get_file_log_mask(void)
{
    if (!atomic_load(&ctx.is_init))
        return 0;

    return atomic_load(&ctx.file_log_mask);
}

int set_cli_log_mask(const unsigned int mask)
{
    if (!atomic_load(&ctx.is_init))
        return LOG_ENOINIT;

    atomic_store(&ctx.cli_log_mask, mask);
    return LOG_SUCCESS;
}

int set_file_log_mask(const unsigned int mask)
{
    if (!atomic_load(&ctx.is_init))
        return LOG_ENOINIT;

    atomic_store(&ctx.file_log_mask, mask);
    return LOG_SUCCESS;
}

int log_write(const char *fmt, ...)
{
    if (!fmt)
        return LOG_EINVALARG;

    if (!atomic_load(&ctx.is_init))
        return LOG_ENOINIT;

    /* Cheap early exit: bail out before formatting if no file is open */
    pthread_mutex_lock(&ctx.mutex);
    if (ctx.log_fd < 0) {
        pthread_mutex_unlock(&ctx.mutex);
        return LOG_ENOINIT;
    }
    pthread_mutex_unlock(&ctx.mutex);

    /* Optional timestamp */
#ifdef LOG_TIMESTAMP
    char time_buf[32];
    time_t now = time(NULL);
    struct tm tm_info;

    if (localtime_r(&now, &tm_info) == NULL) {
        errno = EFAULT;
        return LOG_ESYS;
    }

    if (strftime(time_buf, sizeof(time_buf), LOG_TIMESTAMP_FMT, &tm_info) ==
        0) {
        errno = EINVAL;
        return LOG_ESYS;
    }
#endif

    /* Format the user message (silently truncated to the buffer size) */
    char msg_buf[LOG_MSG_BUF_SIZE];
    va_list args;

    va_start(args, fmt);
    int msg_len = vsnprintf(msg_buf, sizeof(msg_buf), fmt, args);
    va_end(args);

    if (msg_len < 0) {
        errno = EILSEQ;
        return LOG_ESYS;
    }

    if (msg_len > 0 && msg_buf[msg_len - 1] == '\n') {
        msg_buf[msg_len - 1] = '\0';
    }

    /* Assemble the final line */
    char full_line[LOG_LINE_BUF_SIZE];
    int line_len;

#ifdef LOG_TIMESTAMP
    line_len = snprintf(
        full_line, sizeof(full_line), "[ %s ] %s\n", time_buf, msg_buf);
#else
    line_len = snprintf(full_line, sizeof(full_line), "%s\n", msg_buf);
#endif

    if (line_len < 0 || (size_t)line_len >= sizeof(full_line)) {
        errno = EOVERFLOW;
        return LOG_ESYS;
    }

    /* Critical section: re-check the fd (it may have changed since the
     * early exit above) and perform the guaranteed write */
    pthread_mutex_lock(&ctx.mutex);

    if (ctx.log_fd < 0) {
        pthread_mutex_unlock(&ctx.mutex);
        return LOG_ENOINIT;
    }

    bool success = write_all(ctx.log_fd, full_line, (size_t)line_len);

    pthread_mutex_unlock(&ctx.mutex);

    if (!success) {
        return LOG_ESYS;
    }

    return LOG_SUCCESS;
}

int print_colored_log(const char *filepath)
{
    if (!filepath) {
        errno = EINVAL;
        return -1;
    }

    FILE *fp = fopen(filepath, "r");
    if (!fp) {
        perror("fopen log_file");
        return -1;
    }

    char *line = NULL;
    size_t len = 0;
    ssize_t nread;

    /* Colors are only emitted when stdout is a terminal */
    int use_color = isatty(STDOUT_FILENO);

    while ((nread = getline(&line, &len, fp)) != -1) {
        const char *color = RESET;
        FILE *target_stream = stdout;

        /* Find the log level this line belongs to */
        for (size_t i = 0; i < LOG_LEVELS_NR; i++) {
            if (strstr(line, LOG_LEVELS[i].status)) {
                color = LOG_LEVELS[i].tty_color;
                target_stream = (LOG_LEVELS[i].stream_type == STREAM_STDERR)
                                    ? stderr
                                    : stdout;
                break;
            }
        }

#ifdef LOG_TTY_COLOR
        if (use_color) {
            /* Colorized output on the level's stream */
            fprintf(target_stream, "%s%s" RESET, color, line);
        } else {
            /* Output to a pipe or file: no ANSI codes */
            fputs(line, target_stream);
        }
#else
        /* Colors are disabled at build time */
        (void)color;
        (void)use_color;
        fputs(line, target_stream);
#endif
    }

    free(line);
    fclose(fp);
    return 0;
}