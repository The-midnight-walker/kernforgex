// SPDX-License-Identifier: GPL-2.0
//
// vim: set ts=8 sw=8 noet tw=80 cc=80 fo+=t :ù

#include "clicntl.h"

int main(int argc, char **argv)
{
    struct cli_ctx ctx;

    /* Initialize command line context and default options */
    if (cli_ctx_init(&ctx, argc, argv)) {
        pr_error("failed to initialize cli context");
        return -1;
    }

    /* Parsing */
    if (cli_parser(&ctx)) {
        pr_error("failed to parse command line");
        return -1;
    }

    /* Handling options and actions */
    if (handle(&ctx)) {
        pr_error("failed to handle command line arguments");
        return -1;
    }

    pr_debug("program end...");
    return 0;
}