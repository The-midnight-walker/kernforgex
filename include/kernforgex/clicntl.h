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
#include <string.h>

#define INIT_FLAG(flag_struct_ptr, l_opt, has_arg_val, s_opt)                  \
    do {                                                                       \
        (flag_struct_ptr)->is_set = 0;                                         \
        (flag_struct_ptr)->opt.name = (l_opt);                                 \
        (flag_struct_ptr)->opt.has_arg = (has_arg_val);                        \
        (flag_struct_ptr)->opt.flag = &(flag_struct_ptr)->is_set;              \
        (flag_struct_ptr)->opt.val = (s_opt);                                  \
    } while (0)

#define INIT_OPTION(opt_ptr, long_name, has_arg_val, short_name)               \
    do {                                                                       \
        (opt_ptr)->is_set = 0;                                                 \
        (opt_ptr)->val = NULL;                                                 \
        (opt_ptr)->opt.name = (long_name);                                     \
        (opt_ptr)->opt.has_arg = (has_arg_val);                                \
        (opt_ptr)->opt.flag = &(opt_ptr)->is_set;                              \
        (opt_ptr)->opt.val = (short_name);                                     \
    } while (0)

#define RESET_OPTION(opt_ptr)                                                  \
    do {                                                                       \
        (opt_ptr)->is_set = 0;                                                 \
        if ((opt_ptr)->val) {                                                  \
            free((void *)(opt_ptr)->val);                                      \
            (opt_ptr)->val = NULL;                                             \
        }                                                                      \
    } while (0)

#define SET_OPTION(opt_ptr, new_val)                                           \
    do {                                                                       \
        if ((opt_ptr)->val) {                                                  \
            free((void *)(opt_ptr)->val);                                      \
        }                                                                      \
        (opt_ptr)->is_set = 1;                                                 \
        (opt_ptr)->val = (new_val) ? strdup(new_val) : NULL;                   \
    } while (0)

typedef struct option_struct {
    const char *val;
    struct option opt;
} opt_t;

typedef struct flag_struct {
    int is_set;
    struct option opt;
} flag_t;

/*kernel debug */
#define HELP_S_OPT 'h'
#define HELP_L_OPT "help"
/*verbose*/
#define VERBOSE_S_OPT 'v'
#define VERBOSE_L_OPT "verbose"
/*kernel debug */
#define KERN_DBG_S_OPT 'd'
#define KERN_DBG_L_OPT "kern-dbg"
/*packages */
#define PKG_S_OPT 'p'
#define PKG_L_OPT "packages"
/*removing */
#define RM_S_OPT 'r'
#define RM_L_OPT "remove"
/*set aliases */
#define ALIAS_S_OPT 'a'
#define ALIAS_L_OPT "aliases"
/*set files */
#define FILES_S_OPT 'f'
#define FILES_L_OPT "files"
/*vimrc */
#define VIMRC_S_OPT 'c'
#define VIMRC_L_OPT "vimrc"
/*makefile */
#define MAKEFILE_S_OPT 'm'
#define MAKEFILE_L_OPT "makefile"
/*install*/
#define VERBOSE_S_OPT 'v'
#define VERBOSE_L_OPT "verbose"

/* scripts files path*/
/* kernel debuging */
#ifndef KERN_DBG_SH_PATH
#define KERN_DBG_SH_PATH ""
#endif

struct cli_config_struct {
    flag_t help;
    flag_t verbose;
    flag_t remove;
    flag_t packages;
    flag_t aliases;
    flag_t files;
    flag_t makefile;
    flag_t vimrc;
    flag_t kern_dbg;
};

struct cli_ctx {
    const int argc;
    char *const *argv;
    struct cli_config_struct cfg;
};

int cli_parser(struct cli_ctx *);
int usage(FILE *stream, const char *prog_name);
int init_cli_config(struct cli_config_struct *cfg);

/*-----------| handlers */
int handle(struct cli_config_struct *);
int debug_kernel_handle(char *const[], [[maybe_unused]] void *);
int execve_shell_script(const char *, char *const[]);

#endif /*INCLUDE_KERNFORGEX_H*/
