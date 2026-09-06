// SPDX-License-Identifier: GPL-2.0
//
// vim: set ts=8 sw=8 noet tw=80 cc=80 fo+=t :

#ifndef INCLUDE_DEBUG_H
#define INCLUDE_DEBUG_H

#include <stdio.h>

#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN "\033[36m"
#define RESET "\033[0m"

#ifndef prfx_fmt
#define prfx_fmt ""
#endif

#ifndef COLOR_PRFX_FMT
#define COLOR_PRFX_FMT YELLOW
#endif

#ifndef COLOR_META_FMT
#define COLOR_META_FMT GREEN
#endif

/**
 * @brief Log levels used by the logging system.
 *
 * Each log level is represented by a bit flag and can be combined
 * using bitwise operations with dbg_loglevel define in include/debug.c.
 *
 * By default, all log levels are enabled in dbg_loglevel.
 *
 * @note The pr_foo debug log levels are enabled only when DEBUG is defined.
 */

#define DBG_LOGLEVEL_FATAL 0x01
#define DBG_LOGLEVEL_WARN 0x02
#define DBG_LOGLEVEL_ERROR 0x04
#define DBG_LOGLEVEL_INFO 0x08
#define DBG_LOGLEVEL_DEBUG 0x10

/**
 * @brief Functions for managing the logging level mask.
 *
 * These functions allow retrieving, setting, enabling, and disabling
 * individual or multiple log levels using bitwise operations.
 *
 * @note Each log level is represented by a bit flag.
 */
unsigned int get_debug_loglevel(void);
unsigned int set_debug_loglevel(const unsigned int);
unsigned int disable_debug_loglevel(const unsigned int);
unsigned int enable_debug_loglevel(const unsigned int);

/**
 * @brief output format in debugging mode : [ status ] prfx_fmt: fmt [ function
 * : file:line]
 */
#ifdef DEBUG
#define dbg_log(status, fmt, ...)                                              \
    do {                                                                       \
        fprintf(                                                               \
            stderr,                                                            \
            "[ %s ] " COLOR_PRFX_FMT prfx_fmt RESET fmt,                       \
            status,                                                            \
            ##__VA_ARGS__);                                                    \
        fprintf(                                                               \
            stderr,                                                            \
            COLOR_META_FMT " [ %s : %s:%d ] \n" RESET,                         \
            __func__,                                                          \
            (strncmp(__FILE__, "./", 2) == 0 ? __FILE__ + 2 : __FILE__),       \
            __LINE__);                                                         \
        fflush(stderr);                                                        \
    } while (0)
#else
#define dbg_log(...) ((void)0)
#endif /*DEBUG*/

#define pr_fatal(fmt, ...)                                                     \
    do {                                                                       \
        if (get_debug_loglevel() & DBG_LOGLEVEL_FATAL)                         \
            dbg_log(RED "FATAL" RESET, fmt, ##__VA_ARGS__);                    \
    } while (0)

#define pr_warn(fmt, ...)                                                      \
    do {                                                                       \
        if (get_debug_loglevel() & DBG_LOGLEVEL_WARN)                          \
            dbg_log(RED "WARN " RESET, fmt, ##__VA_ARGS__);                    \
    } while (0)

#define pr_error(fmt, ...)                                                     \
    do {                                                                       \
        if (get_debug_loglevel() & DBG_LOGLEVEL_ERROR)                         \
            dbg_log(RED "ERROR" RESET, fmt, ##__VA_ARGS__);                    \
    } while (0)

#define pr_info(fmt, ...)                                                      \
    do {                                                                       \
        if (get_debug_loglevel() & DBG_LOGLEVEL_INFO)                          \
            dbg_log(YELLOW "INFO " RESET, fmt, ##__VA_ARGS__);                 \
    } while (0)

#define pr_debug(fmt, ...)                                                     \
    do {                                                                       \
        if ((get_debug_loglevel()) & DBG_LOGLEVEL_DEBUG)                       \
            dbg_log(GREEN "DEBUG" RESET, fmt, ##__VA_ARGS__);                  \
    } while (0)

#endif /*INCLUDE_DEBUG_H*/