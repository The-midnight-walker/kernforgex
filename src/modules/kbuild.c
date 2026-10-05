// SPDX-License-Identifier: GPL-2.0
//
// vim: set ts=8 sw=8 noet tw=80 cc=80 fo+=t :

/**
 * @file      kbuild.c
 * @author    midnight walker
 * @brief     Linux kernel building and overall low-level
 *            system engineering handle module implementation
 *
 * @version   0.1
 * @date      2026-10-03
 * @copyright GNU General Public License v2.0
 *
 */

#define prfx_fmt "module-kbuild: "

#include <stdlib.h>

#include "debug.h"
#include "modules.h"
#include "shell.h"

static const module_t *kbuild_module = NULL;

enum kbuild_module_opts {
    OPT_HELP,
    OPT_VERBOSE,
    OPT_PACKAGE,
    OPT_INSTALL,
    OPT_REMOVE,
    OPT_LIST,
    OPT_COLOR,

    /*sentinel value for the end of the array */
    OPTS_NR
};

int kbuild_action(int optc, module_option_t **optv)
{
    pr_debug("Executing action for module '%s'", KBUILD_MODULE_NAME);

    char *sh_args[optc + 2];
    char opt_bufs[optc > 0 ? optc : 1][3];

    size_t idx = 0;
    size_t buf_idx = 0;
    int ret = 0;

    sh_args[idx++] = (char *)KBUILD_MODULE_NAME;

    if (0 == optc || !optv || !*optv) {
        pr_module_usage(stdout, kbuild_module);
        return -1;
    }

    for (int i = 0; i < optc; i++) {
        module_option_t *o = optv[i];

        if (!o)
            continue;

        if (o->s_opt == 'h' || (o->l_opt && strcmp(o->l_opt, "help") == 0)) {
            pr_module_usage(stdout, kbuild_module);
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

    ret = execve_shell_script(KBUILD_SH, sh_args);

    if (2 == ret)
        pr_module_usage(stderr, kbuild_module);

    return ret;
}

int init_module_kbuild()
{
    module_option_t kbuild_opts[OPTS_NR] = {
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
        [OPT_COLOR] =
            {.s_opt = 'c',
             .l_opt = "color",
             .has_arg = no_argument,
             .arg_name = NULL,
             .desc = "Color output text",
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

    kbuild_module = register_module(
        KBUILD_MODULE_NAME,
        "[-v|--verbose] [-c|--color] [-h|--help] [-p|--packages [-i|--install] "
        "[-r|--remove] [-l|--list] ]",
        kbuild_opts,
        OPTS_NR,
        kbuild_action);

    if (!kbuild_module) {
        return -1;
    }

    return 0;
}