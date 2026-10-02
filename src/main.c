// SPDX-License-Identifier: GPL-2.0
//
// vim: set ts=8 sw=8 noet tw=80 cc=80 fo+=t :

/**
 * @file      main.c
 * @author    midnight walker
 * @brief     Main entry point for the kfgx CLI tool.
 *
 * @version   0.4
 * @date      2026-10-02
 * @copyright GNU General Public License v2.0
 *
 */

#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "debug.h"
#include "modules.h"

int main(int argc, char **argv)
{
    int ret;
    (void)argc;

    /* Logging Subsystem Initialization */
    ret = init_logging_ctx(
        LOG_CLI_MASK,
        LOG_FILE_PATHNAME,
        LOG_FILE_FLAGS,
        LOG_FILE_MODE,
        LOG_FILE_MASK);

    if (ret != LOG_SUCCESS) {
        pr_error(
            "Failed to initialize file logger [ '%s', flags: %d, mode: %o ] "
            "(code: %d)",
            LOG_FILE_PATHNAME,
            LOG_FILE_FLAGS,
            LOG_FILE_MODE,
            ret);
        return ret;
    } else {
        pr_info("Logging subsystem initialized successfully");
    }

    /*
     * Static array of function pointers pointing to the initialization
     * routines of each subsystem. This ensures an extensible and clean
     * linear bootstrap sequence.
     */
    int (*init_module[])(void) = {
        init_module_kfgx,
        init_module_kerndebug,
    };

    for (size_t i = 0; i < sizeof(init_module) / sizeof(init_module[0]); i++) {
        if (init_module[i]() != 0) {
            pr_error("Module initialization failed at index %zu", i);
            return -1;
        }
    }

    /* CLI Dispatch & Launch */
    ret = launch_cli(argv);

    return ret;
}