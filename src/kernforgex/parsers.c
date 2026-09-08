// SPDX-License-Identifier: GPL-2.0
//
// vim: set ts=8 sw=8 noet tw=80 cc=80 fo+=t :

/**
 * @file      parsers.c
 * @author    midnight walker
 * @brief     Command line options and program configuration files parsing.
 * @version   0.1
 * @date      2026-09-06
 * @copyright GNU General Public License v2.0
 */

#define pr_prfx "parser: "

#define _GNU_SOURCE
#include "clicntl.h"
#include <ctype.h>
#include <getopt.h>
#include <stdlib.h>

#define printable(ch) (isprint((unsigned char)ch) ? ch : '#')
#define FLAG_CASE(opt, str_debug, s_opt)                                       \
    case (s_opt):                                                              \
        (opt).is_set = 1;                                                      \
        pr_debug(str_debug " flag is enable");                                 \
        break

static void parser_usage_error(char *prog_name, char *msg, int opt)
{
    if (msg != NULL && opt != 0)
        fprintf(stderr, "%s (-%c)\n", msg, printable(opt));
    usage(stderr, prog_name);
    exit(-1);
}

static int kfgx_cli_parser_impl(struct cli_ctx *ctx)
{
    int opt;
    struct option options[] = {
        ctx->cfg.help.opt,
        ctx->cfg.verbose.opt,
        ctx->cfg.kern_dbg.opt,
        ctx->cfg.files.opt,
        ctx->cfg.makefile.opt,
        ctx->cfg.vimrc.opt,
        ctx->cfg.packages.opt,
        ctx->cfg.aliases.opt,
        ctx->cfg.remove.opt,

        /* sentinel */
        {0, 0, 0, 0}};

    pr_debug("parsing command line");

    while ((opt = getopt_long(
                ctx->argc,
                ctx->argv,
                (char[]){HELP_S_OPT,
                         VERBOSE_S_OPT,
                         KERN_DBG_S_OPT,
                         PKG_S_OPT,
                         VIMRC_S_OPT,
                         ALIAS_S_OPT,
                         MAKEFILE_S_OPT,
                         FILES_S_OPT,
                         RM_S_OPT,
                         '\0'},
                options,
                NULL)) != -1) {
        pr_debug("opt=%4d (%c); optind = %d", opt, printable(opt), optind);

        switch (opt) {

            FLAG_CASE(ctx->cfg.help, "help", HELP_S_OPT);
            FLAG_CASE(ctx->cfg.verbose, "verbose", VERBOSE_S_OPT);
            FLAG_CASE(ctx->cfg.kern_dbg, "kernel debuging", KERN_DBG_S_OPT);
            FLAG_CASE(ctx->cfg.packages, "packages", PKG_S_OPT);
            FLAG_CASE(ctx->cfg.vimrc, "vimrc", VIMRC_S_OPT);
            FLAG_CASE(ctx->cfg.aliases, "aliases", ALIAS_S_OPT);
            FLAG_CASE(ctx->cfg.makefile, "makefile", MAKEFILE_S_OPT);
            FLAG_CASE(ctx->cfg.files, "files", FILES_S_OPT);
            FLAG_CASE(ctx->cfg.remove, "removing", RM_S_OPT);

        /* */
        case '?':
            parser_usage_error(ctx->argv[0], "Unrecognized argument", optopt);
            break;

        case ':':
            parser_usage_error(ctx->argv[0], "Missing argument", optopt);
            break;

        default:
            pr_fatal("Unexpected case in switch");
            exit(-1);
            break;
        }
    }

    pr_debug("parsing end");
    return 0;
}

int cli_parser(struct cli_ctx *ctx)
{
    if (!ctx) {
        pr_error("cli_ctx=%p", (void *)ctx);
        return -1;
    }

    if (ctx->argc == 0) {
        pr_debug("argc=0");
        return -1;
    }

    if (!ctx->argv || !*ctx->argv) {
        pr_debug(
            "argv=%p, argv[0]=%p\n",
            ctx->argv,
            (ctx->argv && *ctx->argv) ? *ctx->argv : NULL);
        return -1;
    }

    if (!ctx->argv[1])
        pr_info("execute program with no options");

    if (!ctx->envp || !*ctx->envp)
        pr_debug(
            "argv=%p, argv[0]=%p\n",
            ctx->envp,
            (ctx->envp && *ctx->envp) ? *ctx->envp : NULL);

    return kfgx_cli_parser_impl(ctx);
}
