// SPDX-License-Identifier: GPL-2.0
//
// vim: set ts=8 sw=8 noet tw=80 cc=80 fo+=t :

#ifndef INCLUDE_MODULE_H
#define INCLUDE_MODULE_H

#ifndef DFLT_PROG_NAME
#define DFLT_PROG_NAME "kfgx"
#endif

#ifndef SCRIPT_DIR
#define SCRIPT_DIR "/bin/" DFLT_PROG_NAME
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

/**
 * @brief Disable linear tree chaining logic for modules.
 *
 * When defined, this macro deactivates the fallback or legacy linear flattening
 * mechanism for module trees, enforcing strictly hierarchical parent-child
 * relationship resolution during command-line parsing and option evaluation.
 *
 * @{
 */
#define CLI_ENABLE_MODULES_LINEAR_TREE_CHAINING
/** @} */

#include <generic/modules.h>

/**
 * @brief Initializes the kfgx module
 *
 * -----------------------------------------------------------------------------
 * kfgx (root/default) Module
 *
 * Usage: [-h| --help]
 * -----------------------------------------------------------------------------
 */
int init_module_kfgx(void);

#define KDBG_MODULE_NAME "kdbg"

#ifndef KDBG_SH
#define KDBG_SH SCRIPT_DIR "/kerndebug.sh"
#endif
/**
 * @brief Initializes the kdbg module
 *
 * -----------------------------------------------------------------------------
 * kdbg  Module for kernel debugging environment setup and management
 *
 * Usage: [-v|--verbose] [-h|--help] [-c|--color] [-p|--packages [-i|--install]
 * [-r|--remove] [-l|--list] ]
 * -----------------------------------------------------------------------------
 */
int init_module_kdbg(void);

#define KSYS_MODULE_NAME "ksys"

#ifndef KSYS_SH
#define KSYS_SH SCRIPT_DIR "/kernsys.sh"
#endif
/**
 * @brief Initializes the ksys module
 *
 * -----------------------------------------------------------------------------
 * ksys  Module for linux system handle module implementation.
 *
 * Usage: [-v|--verbose] [-h|--help] [-c|--color] [-p|--packages [-i|--install]
 * [-r|--remove] [-l|--list] ]
 * -----------------------------------------------------------------------------
 */
int init_module_ksys(void);

#define KBUILD_MODULE_NAME "kbuild"

#ifndef KBUILD_SH
#define KBUILD_SH SCRIPT_DIR "/kernbuild.sh"
#endif
/**
 * @brief Initializes the kerndebug module
 *
 * -----------------------------------------------------------------------------
 * kbuild  Module for Linux kernel building and overall low-level
 * system engineering handle module implementation
 *
 * Usage: [-v|--verbose] [-h|--help] [-c|--color] [-p|--packages [-i|--install]
 * [-r|--remove] [-l|--list] ] [-f|--files [-s|--set] [-l|--list] ]
 * -----------------------------------------------------------------------------
 */
int init_module_kbuild(void);

#endif /* INCLUDE_MODULE_H */