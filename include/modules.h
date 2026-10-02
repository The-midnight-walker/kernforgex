// SPDX-License-Identifier: GPL-2.0
//
// vim: set ts=8 sw=8 noet tw=80 cc=80 fo+=t :

#ifndef INCLUDE_MODULE_H
#define INCLUDE_MODULE_H

#ifndef DFLT_PROG_NAME
#define DFLT_PROG_NAME "kfgx"
#endif

/**
 * @name Command Chaining Configuration
 * @brief Command chaining support: allows falling back to top-level submodules
 *        when a requested module name is not found in the current nested scope.
 * @details Controlled via CLI_ENABLE_MODULES_CHAINING (0 by default, 1 to
 * enable).
 * @{
 */
// #define CLI_ENABLE_MODULES_CHAINING
/** @} */

/**
 * @name Module Error Handling Configuration
 * @brief Controls error resilience during module execution sequence.
 * @details Controlled via CLI_IGNORE_MODULE_ERRORS (0 by default, 1 to enable).
 *          When enabled, the engine continues processing remaining modules
 *          even if a module's action handler returns a failure code.
 * @{
 */
// #define CLI_IGNORE_MODULE_ERRORS
/** @} */

#include <generic/modules.h>

/**
 * @brief Initializes the kfgx module
 *
 * --------------------------------------------------------------------
 * kfgx (root/default) Module
 *
 * Usage: [-v|--verbose] [-h| --help]
 * ---------------------------------------------------------------------
 */
int init_module_kfgx(void);

#endif /* INCLUDE_MODULE_H */