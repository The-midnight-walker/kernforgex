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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "clicntl.h"
#include <ctype.h>
#include <getopt.h>
#include <stdlib.h>

#define printable(ch) (isprint((unsigned char)(ch)) ? (ch) : '#')

static int cli_parser_impl(struct cli_ctx *ctx)
{
    struct option long_options[OPT_COUNT + 1];
    char optstring[OPT_COUNT * 3 + 2];
    size_t optstr_idx = 0;
    int opt;

    /* Build optstring and long_options dynamically from ctx->opts */
    optstring[optstr_idx++] =
        ':'; /* Leading colon for distinct missing-arg detection */

    for (size_t i = 0; i < ctx->opts_count; i++) {
        cli_opt_t *o = &ctx->opts[i];

        /* Build long_options entry */
        long_options[i].name = o->l_opt;
        long_options[i].has_arg = o->has_arg;
        long_options[i].flag = NULL;
        long_options[i].val =
            o->s_opt ? (int)(unsigned char)o->s_opt : (int)(1000 + i);

        /* Build short optstring */
        if (o->s_opt != 0) {
            optstring[optstr_idx++] = o->s_opt;
            if (o->has_arg == required_argument) {
                optstring[optstr_idx++] = ':';
            } else if (o->has_arg == optional_argument) {
                optstring[optstr_idx++] = ':';
                optstring[optstr_idx++] = ':';
            }
        }
        long_options[i].val =
            o->s_opt ? (int)(unsigned char)o->s_opt : (int)(1000 + i);
    }

    /* Long options sentinel */
    long_options[ctx->opts_count] = (struct option){0};
    optstring[optstr_idx] = '\0';

    pr_debug(
        "generated optstring='%s' for %zu options", optstring, ctx->opts_count);

    /* Reset getopt internal state */
    optind = 1;
    opterr = 0; /* Handle errors explicitly */

    int long_idx = -1;
    while ((opt = getopt_long(
                ctx->argc, ctx->argv, optstring, long_options, &long_idx)) !=
           -1) {
        pr_debug(
            "parsed opt=%d ('%c'), optind=%d", opt, printable(opt), optind);

        if (opt == '?') {
            print_red(
                stderr,
                "Error: Unrecognized option: '%c'\n",
                printable(optopt));
            usage(stderr, ctx->argv[0], ctx);
            return -1;
        }

        if (opt == ':') {
            fprintf(
                stderr,
                RED "Error: (Option requires an argument: '-%c'\n" RESET,
                printable(optopt));
            usage(stderr, ctx->argv[0], ctx);
            return -1;
        }

        /* Find matching option in ctx->opts */
        cli_opt_t *matched = NULL;
        if (long_idx >= 0 && (size_t)long_idx < ctx->opts_count) {
            matched = &ctx->opts[long_idx];
        } else {
            for (size_t i = 0; i < ctx->opts_count; i++) {
                if (ctx->opts[i].s_opt &&
                    (int)(unsigned char)ctx->opts[i].s_opt == opt) {
                    matched = &ctx->opts[i];
                    break;
                }
            }
        }

        if (matched) {
            matched->is_set = true;
            matched->arg_val = optarg;
            pr_debug(
                "matched option '--%s' (-%c), val='%s'",
                matched->l_opt ? matched->l_opt : "",
                matched->s_opt ? matched->s_opt : ' ',
                matched->arg_val ? matched->arg_val : "none");
        } else {
            pr_error("unhandled option code: %d", opt);
            return -1;
        }

        long_idx = -1;
    }

    pr_debug("parsing complete successfully");
    return 0;
}

int cli_parser(struct cli_ctx *ctx)
{
    if (check_cli_ctx(ctx)) {
        pr_error("invalid command line context arguments passed");
        return -1;
    }

    return cli_parser_impl(ctx);
}