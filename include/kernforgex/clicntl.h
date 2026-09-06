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
#include <stdio.h>

struct cli_config_struct {
    int verbose;
    int (*usage)(FILE *,const char *);
};

struct cli_ctx {
    const int argc;
    char *const *argv;
    char *const *envp;
    struct cli_config_struct cfg;
};

int cli_parser(struct cli_ctx *);
int usage(FILE *stream,const char *prog_name);
int init_cli_config(struct cli_config_struct *cfg);

#endif /*INCLUDE_KERNFORGEX_H*/
