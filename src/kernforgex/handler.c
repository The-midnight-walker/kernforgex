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

void usage_impl(FILE *stream, const char *prog_name)
{
    fprintf(stream,
        "Usage: %s [OPTIONS]\n\n"
        "Options:\n"
        "  -v, --verbose    Enable verbose mode\n"
        "  -h, --help       Display this help\n",
        prog_name
    );
}

int usage(FILE *stream,const char *prog_name)
{
    if (!stream) {
        pr_error("stream=%p", (void *)stream);
        return -1;
    }

    if(!prog_name){
        pr_error("prog_name='%p'", (void *)prog_name);
        return -1;
    }

    if (stdin == stream || stderr == stream) {
        usage_impl(stream,prog_name);
        return 0;
    }

    pr_error("invalid stream");
    return -1;
}

int init_cli_config(struct cli_config_struct *cfg)
{
    pr_debug("init command line configuration");
    if (!cfg) {
        pr_error("cli_cfg=%p", (void *)cfg);
        return -1;
    }

    cfg->usage = usage;

    return 0;
}
