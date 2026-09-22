// SPDX-License-Identifier: GPL-2.0
//
// vim: set ts=8 sw=8 noet tw=80 cc=80 fo+=t :

#ifndef INCLUDE_KERNFORGEX_H
#define INCLUDE_KERNFORGEX_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "debug.h"
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declarations */
struct cli_ctx;

/**
 * @brief Identifiers for all supported CLI options.
 *
 * To add a new option, add an enum member before OPT_COUNT
 * and add its descriptor row in default_cli_options[] in handler.c.
 */
typedef enum {
    OPT_HELP,
    OPT_VERBOSE,
    OPT_KERN_DBG,
    OPT_PACKAGES,
    OPT_REMOVE,
    OPT_ALIASES,
    OPT_FILES,
    OPT_VIMRC,
    OPT_MAKEFILE,
    OPT_COUNT
} cli_opt_id_t;

/**
 * @brief Declarative option descriptor and runtime state.
 */
typedef struct cli_opt {
    cli_opt_id_t id;   /**< Unique option identifier */
    char s_opt;        /**< Short flag character (e.g. 'h'), or 0 if none */
    const char *l_opt; /**< Long flag string (e.g. "help"), or NULL if none */
    int has_arg;       /**< no_argument, required_argument, optional_argument */
    const char
        *arg_name; /**< Argument placeholder name for usage (e.g. "<path>") */
    const char *desc; /**< Human-readable help description */

    /* Runtime state (populated during parsing) */
    bool is_set; /**< True if the option was matched on the CLI */
    const char
        *arg_val; /**< Captured argument value (if has_arg != no_argument) */

    /* Optional action callback */
    int (*action)(struct cli_ctx *ctx, struct cli_opt *opt);
} cli_opt_t;

/**
 * @brief Command line context holding arguments and option states.
 */
struct cli_ctx {
    int argc;
    char *const *argv;
    cli_opt_t opts[OPT_COUNT];
    size_t opts_count;
};

/* Assets and scripts directories */
#ifndef CONFIG_ROOT_DIR
#define CONFIG_ROOT_DIR "./assets/"
#endif
#ifndef SCRIPTS_ROOT_DIR
#define SCRIPTS_ROOT_DIR CONFIG_ROOT_DIR "scripts/"
#endif
#ifndef KERN_DBG_SH_PATH
#define KERN_DBG_SH_PATH SCRIPTS_ROOT_DIR "kdbg.sh"
#endif

/* Validation helpers */
static inline int check_script_pathname(const char *script)
{
    if (!script) {
        pr_error("script pathname is undefined");
        return -1;
    }
    if (strlen(script) == 0) {
        pr_error("script pathname is empty");
        return -1;
    }
    return 0;
}

#define GET_FLAG_NAME(f) ((f).is_set ? (char *)((f).opt.name) : NULL)

/*TODO:
Compile-time type check enforcing strict 'struct cli_ctx *' usage
 define CHECK_CLI_CTX_TYPE(ptr) \
   _Generic((ptr), \
        struct cli_ctx *: (ptr), \
       const struct cli_ctx *: (ptr) \
   )
   */

static inline int check_cli_ctx(const struct cli_ctx *ctx)
{
    if (!ctx) {
        pr_error("cli_ctx is NULL");
        return -1;
    }
    if (ctx->argc == 0) {
        pr_debug("argc=0");
        return -1;
    }
    if (!ctx->argv || !*ctx->argv) {
        pr_debug("argv is NULL or empty");
        return -1;
    }
    return 0;
}

/* Query helpers */
static inline bool cli_has_flag(const struct cli_ctx *ctx, cli_opt_id_t id)
{
    if (!ctx || id >= OPT_COUNT)
        return false;
    return ctx->opts[id].is_set;
}

static inline const char *
cli_get_arg(const struct cli_ctx *ctx, cli_opt_id_t id)
{
    if (!ctx || id >= OPT_COUNT)
        return NULL;
    return ctx->opts[id].arg_val;
}

static inline cli_opt_t *cli_get_opt(struct cli_ctx *ctx, cli_opt_id_t id)
{
    if (!ctx || id >= OPT_COUNT)
        return NULL;
    return &ctx->opts[id];
}

/* Public API functions */
int cli_ctx_init(struct cli_ctx *ctx, int argc, char *const *argv);
int cli_parser(struct cli_ctx *ctx);
int usage(FILE *stream, const char *prog_name, const struct cli_ctx *ctx);
int handle(struct cli_ctx *ctx);
int execve_shell_script(const char *pathname, char *const argv[]);

#endif /* INCLUDE_KERNFORGEX_H */
