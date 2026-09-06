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

static void
parser_usage_error(struct cli_config_struct *cfg,char *prog_name, char *msg, int opt)
{
    if (msg != NULL && opt != 0)
        fprintf(stderr, "%s (-%c)\n", msg, printable(opt));
    cfg->usage(stderr,prog_name);
    exit(-1);
}

static int kfgx_cli_parser_impl(struct cli_ctx *ctx)
{
    int opt;
    static struct option options[] = {
        {"verbose", no_argument, 0, 'v'},

        /* ssentinel */
        {0, 0, 0, 0}};

    while ((opt = getopt_long(ctx->argc, ctx->argv, "vx:", options, NULL)) !=
           -1) {
        pr_debug("opt=%4d (%c); optind = %d", opt, printable(opt), optind);

        switch (opt) {
        case 'v':
            ctx->cfg.verbose = 1;
            pr_debug("verbose is enable");
            break;

        /* */
        case '?':
            parser_usage_error(&ctx->cfg,ctx->argv[0], "Unrecognized argument", optopt);
            break;

        case ':':
            parser_usage_error(&ctx->cfg,ctx->argv[0], "Missing argument", optopt);
            break;

        default:
            pr_fatal("Unexpected case in switch");
            exit(-1);
            break;
        }
    }
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
