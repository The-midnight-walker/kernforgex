// SPDX-License-Identifier: GPL-2.0
//
// vim: set ts=8 sw=8 noet tw=80 cc=80 fo+=t :

/**
 * @file      kfgx_mod.c
 * @author    midnight walker
 * @brief     Root module implementation for the kernforgex CLI application,
 *            providing the default command-line interface and option parsing.
 * @version   0.1
 * @date      2026-09-30
 * @copyright GNU General Public License v2.0
 *
 */

#define prfx_fmt "module-kfgx: "

#include "debug.h"
#include "modules.h"

static const module_t *kfgx_module = NULL;

enum kfgx_module_opts {
    OPT_HELP,

    /*sentinel value for the end of the array */
    OPTS_NR
};

int kfgx_action(int optc, module_option_t **optv)
{
    (void)optc;
    (void)optv;

    pr_debug("Executing action for module '%s'\n", DFLT_PROG_NAME);

    pr_module_usage(stdout, kfgx_module);

    return 0;
}

int init_module_kfgx()
{
    module_option_t kfgx_opts[OPTS_NR] = {
        [OPT_HELP] =
            {.s_opt = 'h',
             .l_opt = "help",
             .has_arg = no_argument,
             .arg_name = NULL,
             .desc = "Display this help message",
             .arg_val = NULL,
             .is_set = false},
    };

    kfgx_module = set_root_module(
        DFLT_PROG_NAME, "[-h|--help]", kfgx_opts, OPTS_NR, kfgx_action);

    if (!kfgx_module) {
        return -1;
    }

    return 0;
}