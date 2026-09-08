// SPDX-License-Identifier: GPL-2.0
//
// vim: set ts=8 sw=8 noet tw=80 cc=80 fo+=t :

/**
 * @file      handler.c
 * @author    midnight walker
 * @brief     Execute options.
 * @version   0.1
 * @date      2026-09-06
 * @copyright GNU General Public License v2.0
 */

#define pr_prfx "handle: "

#include "clicntl.h"
#include <stdlib.h>

extern char **envrion;

#define IS_OPT_SET(opt, s_opt) ((opt.is_set)) ? (s_opt) : ' '

#define eval_str(str, def) (str) ? str : def

void usage_impl(FILE *stream, const char *prog_name)
{
    fprintf(
        stream,
        "Usage: %s [OPTIONS]\n\n"
        "Options:\n"
        "  -v, --verbose    Enable verbose mode\n"
        "  -h, --help       Display this help\n",
        prog_name);
}

int usage(FILE *stream, const char *prog_name)
{
    if (!stream) {
        pr_error("stream=%p", (void *)stream);
        return -1;
    }

    if (!prog_name) {
        pr_error("prog_name='%p'", (void *)prog_name);
        return -1;
    }

    if (stdout == stream || stderr == stream) {
        usage_impl(stream, prog_name);
        return 0;
    }

    pr_error("invalid stream");
    return -1;
}

int init_cli_config(struct cli_config_struct *cfg)
{
    pr_debug("init command line structure configuration");
    if (!cfg) {
        pr_error("cli_cfg=%p", (void *)cfg);
        return -1;
    }

    INIT_FLAG(&cfg->help, HELP_L_OPT, no_argument, HELP_S_OPT);
    INIT_FLAG(&cfg->verbose, VERBOSE_L_OPT, no_argument, VERBOSE_S_OPT);
    INIT_FLAG(&cfg->kern_dbg, KERN_DBG_L_OPT, no_argument, KERN_DBG_S_OPT);
    INIT_FLAG(&cfg->remove, RM_L_OPT, no_argument, RM_S_OPT);
    INIT_FLAG(&cfg->packages, PKG_L_OPT, no_argument, PKG_S_OPT);
    INIT_FLAG(&cfg->vimrc, VIMRC_L_OPT, no_argument, VIMRC_S_OPT);
    INIT_FLAG(&cfg->makefile, MAKEFILE_L_OPT, no_argument, MAKEFILE_S_OPT);
    INIT_FLAG(&cfg->files, FILES_L_OPT, no_argument, FILES_S_OPT);

    return 0;
}

int clean_cli_cfg(struct cli_config_struct *cfg)
{
    pr_debug("clean command line struct configuration");
    if (!cfg) {
        pr_error("cli_cfg=%p", (void *)cfg);
        return -1;
    }

    return 0;
}

int handle(struct cli_config_struct *cfg)
{
    int ret = -1;
    char *args = NULL;

    pr_debug("handling command line");
    if (!cfg) {
        pr_error("cli_cfg=%p", (void *)cfg);
        return ret;
    }

    make_args(
        args,
        "%c %c %c %c %c",
        IS_OPT_SET(cfg->remove, RM_S_OPT),
        IS_OPT_SET(cfg->verbose, VERBOSE_S_OPT),
        IS_OPT_SET(cfg->packages, PKG_S_OPT),
        IS_OPT_SET(cfg->aliases, ALIAS_S_OPT),
        IS_OPT_SET(cfg->files, FILES_S_OPT));

    /* debug kernel */
    if (cfg->kern_dbg.is_set)
        ret = debug_kernel_handle(args, 0);

    /* usage for help */
    if (cfg->help.is_set)
        ret = usage(stdout, "");

    if (cfg->vimrc.is_set)
        ret = 1;

    destroy_args(args);

    return ret;
}

int debug_kernel_handle(const char *args, [[maybe_unused]] void *data)
{
    if (data) {
        pr_debug("passed a cookie");
    } else {
        pr_debug("no cookie passed data=%p", (void *)data);
    }

    return exec_shell_script(KERN_DBG_SH_PATH, args);
}

int exec_shell_script(const char *script_path, const char *args)
{
    if (!script_path) {
        pr_error(
            "script to execute path didn't have been specified script_path=%p",
            (void *)script_path);
        return -1;
    }

    if (!args)
        pr_debug("args=%p", (void *)args);

    pr_debug("execute shell script");

    return 0;
}
