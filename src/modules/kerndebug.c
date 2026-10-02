// SPDX-License-Identifier: GPL-2.0
//
// vim: set ts=8 sw=8 noet tw=80 cc=80 fo+=t :

/**
 * @file      kerndebug.c
 * @author    midnight walker
 * @brief     Kernel debugging module implementation.
 *
 * @version   0.1
 * @date      2026-10-02
 * @copyright GNU General Public License v2.0
 *
 */

#define prfx_fmt "module-kerndebug: "

#include <stdlib.h>

#include "debug.h"
#include "modules.h"
#include "shell.h"

static const module_t *kerndebug_module = NULL;

enum kerndebug_module_opts {
    OPT_HELP,
    OPT_VERBOSE,
    OPT_PACKAGE,
    OPT_INSTALL,
    OPT_REMOVE,
    OPT_LIST,

    /*sentinel value for the end of the array */
    OPTS_NR
};

int kerndebug_action(int optc, module_option_t **optv)
{
    pr_debug("Executing action for module '%s'", KERDEBUG_MODULE_NAME);

    char *sh_args[optc + 2];
    char opt_bufs[optc > 0 ? optc : 1][3];

    size_t idx = 0;
    size_t buf_idx = 0;
    int ret = 0;

    sh_args[idx++] = (char *)KERDEBUG_MODULE_NAME;

    for (int i = 0; i < optc; i++) {
        module_option_t *o = optv[i];

        if (!o)
            continue;

        if (o->s_opt == 'h' || (o->l_opt && strcmp(o->l_opt, "help") == 0)) {
            pr_module_usage(stdout, kerndebug_module);
            return ret;
        }

        if (o->s_opt) {
            opt_bufs[buf_idx][0] = '-';
            opt_bufs[buf_idx][1] = o->s_opt;
            opt_bufs[buf_idx][2] = '\0';

            sh_args[idx++] = opt_bufs[buf_idx++];
        } else if (o->l_opt) {
            sh_args[idx++] = (char *)o->l_opt;
        }
    }

    sh_args[idx] = NULL;

    ret = execve_shell_script(KERNDEBUG_SH, sh_args);

    if (2 == ret)
        pr_module_usage(stderr, kerndebug_module);

    return ret;
}

int init_module_kerndebug()
{
    module_option_t kerndebug_opts[OPTS_NR] = {
        [OPT_HELP] =
            {.s_opt = 'h',
             .l_opt = "help",
             .has_arg = no_argument,
             .arg_name = NULL,
             .desc = "Display this help message",
             .arg_val = NULL,
             .is_set = false},
        [OPT_VERBOSE] =
            {.s_opt = 'v',
             .l_opt = "verbose",
             .has_arg = no_argument,
             .arg_name = NULL,
             .desc = "Enable verbose logging output",
             .arg_val = NULL,
             .is_set = false},
        [OPT_PACKAGE] =
            {.s_opt = 'p',
             .l_opt = "packages",
             .has_arg = no_argument,
             .arg_name = NULL,
             .desc = "List debugging packages",
             .arg_val = NULL,
             .is_set = false},
        [OPT_INSTALL] =
            {.s_opt = 'i',
             .l_opt = "install",
             .has_arg = no_argument,
             .arg_name = NULL,
             .desc = "Install debugging packages",
             .arg_val = NULL,
             .is_set = false},
        [OPT_REMOVE] =
            {.s_opt = 'r',
             .l_opt = "remove",
             .has_arg = no_argument,
             .arg_name = NULL,
             .desc = "Remove debugging packages",
             .arg_val = NULL,
             .is_set = false},
        [OPT_LIST] = {
            .s_opt = 'l',
            .l_opt = "list",
            .has_arg = no_argument,
            .arg_name = NULL,
            .desc = "List items (packages or files, etc...)",
            .arg_val = NULL,
            .is_set = false}};

    kerndebug_module = register_module(
        KERDEBUG_MODULE_NAME,
        "[-v|--verbose] [-h|--help] [-p|--packages [-i|--install] "
        "[-r|--remove] [-l|--list] ]",
        kerndebug_opts,
        OPTS_NR,
        kerndebug_action);

    if (!kerndebug_module) {
        return -1;
    }

    return 0;
}