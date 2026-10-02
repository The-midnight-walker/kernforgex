// SPDX-License-Identifier: GPL-2.0
//
// vim: set ts=8 sw=8 noet tw=80 cc=80 fo+=t :

#ifndef INCLUDE_DEBUG_H
#define INCLUDE_DEBUG_H

#ifndef LOG_TIMESTAMP
#define LOG_TIMESTAMP
#endif

#ifndef LOG_TTY_COLOR
#define LOG_TTY_COLOR
#endif

#include <generic/debug.h>

#ifdef DEBUG

#define LOG_CLI_MASK                                                           \
    (LOG_LEVEL_FATAL | LOG_LEVEL_WARN | LOG_LEVEL_ERROR | LOG_LEVEL_INFO |     \
     LOG_LEVEL_DEBUG)
#define LOG_FILE_MASK                                                          \
    (LOG_LEVEL_FATAL | LOG_LEVEL_WARN | LOG_LEVEL_ERROR | LOG_LEVEL_INFO |     \
     LOG_LEVEL_DEBUG)

#elif defined(RELEASE)

#define LOG_CLI_MASK (0)
#define LOG_FILE_MASK                                                          \
    (LOG_LEVEL_FATAL | LOG_LEVEL_WARN | LOG_LEVEL_ERROR | LOG_LEVEL_INFO)
#else
/* !defined(DEBUG) && !defined(RELEASE) */
#define LOG_CLI_MASK (0)
#define LOG_FILE_MASK (0)
#endif /* DEBUG */

#ifndef LOG_FILE_PATHNAME
#define LOG_FILE_PATHNAME "kfgx.log"
#endif

#ifndef LOG_FILE_MODE
#define LOG_FILE_MODE (0644)
#endif

#ifndef LOG_FILE_FLAGS
#define LOG_FILE_FLAGS (O_CREAT | O_WRONLY | O_APPEND)
#endif

#ifndef pr_error_to_user
#define pr_error_to_user(fmt, ...) pr_red(stderr, "Error: " fmt, ##__VA_ARGS__)
#endif

#ifndef pr_warn_to_user
#define pr_warn_to_user(fmt, ...) pr_red(stderr, "Warning: " fmt, ##__VA_ARGS__)
#endif

#ifndef pr_info_to_user
#define pr_info_to_user(fmt, ...) pr_yellow(stdout, fmt, ##__VA_ARGS__)
#endif

#ifndef pr_to_user
#define pr_to_user(fmt, ...) pr(stdout, fmt, ##__VA_ARGS__)
#endif

#endif /* INCLUDE_DEBUG_H */