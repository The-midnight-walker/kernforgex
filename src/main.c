// SPDX-License-Identifier: GPL-2.0
//
// vim: set ts=8 sw=8 noet tw=80 cc=80 fo+=t :ù

#include "clicntl.h"

int main(int argc, char **argv)
{
    /*init command line context */
    struct cli_ctx ctx = {
        .argc = argc,
        .argv = argv,
    };

    /*init command line configuration options */
    if (init_cli_config(&ctx.cfg))
        return -1;

    /* parsing */
    if (cli_parser(&ctx)) {
        return -1;
        pr_error("failed to parsing command line");
    }

    if (handle(&ctx.cfg)) {
        return -1;
        pr_error("failed to handle command line arguments");
    }

    pr_debug("program end...");
    return 0;
}