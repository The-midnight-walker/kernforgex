// SPDX-License-Identifier: GPL-2.0
//
// vim: set ts=8 sw=8 noet tw=80 cc=80 fo+=t :ù

#include "clicntl.h"

int main(int argc, char **argv, char **envp)
{
    int ret = 0;

    /*init command line context */
    struct cli_ctx ctx = {
        .argc = argc,
        .argv = argv,
        .envp = envp,
    };

    /*init command line configuration options */
    if (init_cli_config(&ctx.cfg))
        return -1;

    /* parsing */
    ret = cli_parser(&ctx);

    pr_debug("program end...");
    return ret;
}