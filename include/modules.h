// SPDX-License-Identifier: GPL-2.0
//
// vim: set ts=8 sw=8 noet tw=80 cc=80 fo+=t :

#ifndef INCLUDE_MODULE_H
#define INCLUDE_MODULE_H

#include <stdbool.h>
#include <stddef.h>

#include "debug.h"
#include "lists.h"

/**
 * @brief Command chaining support:
 * If the requested module is not found within the current submodule scope,
 * fallback to 'root_sub_modules' to allow execution of top-level commands
 * in a single invocation (e.g., kfgx net -i eth0 disk status).
 */
#define CLI_ENABLE_MODULES_CHAINING

/**
 * @struct module_option_struct
 * @brief Represents a CLI option associated with a module.
 */
typedef struct module_option_struct {
    char s_opt;           /**< Short flag character (e.g. 'h')*/
    const char *l_opt;    /**< Long flag string (e.g. "help")*/
    int has_arg;          /**< Argument requirement (e.g.
                             no_argument,required_argument)
                             */
    const char *arg_name; /**< Argument placeholder name
                            for usage (e.g. "<path>")
                            */
    const char *desc;     /**< Human-readable help description */

    /* Runtime state (populated during parsing) */
    bool is_set;         /**< True if the option was matched on the CLI */
    const char *arg_val; /**< Captured argument value
                        (if has_arg != no_argument)
                        */
} module_option_t;

/**
 * @struct module_struct
 * @brief Public structure representing a module and its configured options.
 */
typedef struct module_struct {
    char *name;                /**< Module name (heap-allocated) */
    module_option_t **options; /**< Array of pointers to the module's options */
    unsigned int opt_nr;       /**< Total number of options associated with
                                  this module
                                  */
    const char *usage; /**< Usage syntax template (e.g. "[options] <target>",
                        module name is prefixed automatically)
                        */
} module_t;

/**
 * @struct module_entry_struct
 * @brief Private internal structure for hierarchical management and parsing
 * state.
 */
typedef struct module_entry_struct {
    module_t mod; /**< Embedded public structure (allows container_of usage) */

    /* Dynamic array for arguments captured from argv */
    char **args; /**< Dynamic array holding pointers to captured arguments */
    unsigned int args_nr; /**< Number of captured arguments */

    bool matched; /**< True if this module was activated during CLI parsing */
    unsigned int level; /**< Depth level of the module in the tree hierarchy
                            (0 = root) */

    struct module_entry_struct *parent; /**< Pointer to the parent
                                        module, or NULL if root
                                         */
    list_node_t sub_modules; /**< Head of the intrusive list for child
                                sub-modules
                                */
    list_node_t siblings;    /**< Intrusive list node for modules
                               at the same level
                                */
    list_node_t matches; /**< Intrusive list node chaining matched modules */
} module_entry_t;

/**
 * @brief Converts a pointer from a public module_t to its internal
 * module_entry_t container.
 *
 * Uses the container_of macro to calculate the offset of the embedded 'mod'
 * member within the private module_entry_t structure.
 *
 * @param user_mod_ptr Pointer to the public module_t structure.
 *
 * @return Pointer to the enclosing module_entry_t container structure.
 */
#define to_module_entry(user_mod_ptr)                                          \
    container_of(user_mod_ptr, module_entry_t, mod)

/**
 * @brief Registers a new root module (Level 0) in the hierarchy.
 *
 * Allocates and initializes a root-level module entry, then appends it to the
 * global root sub-modules list.
 *
 * @param[in] name    Name of the root module. Must not be NULL.
 * @param[in] usage   Usage syntax string, or NULL if none.
 * @param[in] options Array of option structure pointers, or NULL if none.
 * @param[in] opt_nr  Total number of options in the array.
 *
 * @return Pointer to the embedded public module_t structure, or NULL on
 * failure.
 */
module_t *add_module(
    const char *name,
    const char *usage,
    module_option_t **options,
    unsigned int opt_nr);

/**
 * @brief Allocates or updates the standalone global root module for top-level
 * options.
 *
 * @param[in] name    Name of the application/root entry. Must not be NULL.
 * @param[in] usage   Usage syntax pattern (e.g. "[options] <command>"), or
 * NULL.
 * @param[in] options Array of option structure pointers, or NULL if none.
 * @param[in] opt_nr  Total number of options in the array.
 *
 * @return Pointer to the embedded public module_t structure, or NULL on
 * failure.
 */
module_t *set_root_module(
    const char *name,
    const char *usage,
    module_option_t **options,
    unsigned int opt_nr);

/**
 * @brief Registers a sub-module under a specific parent module.
 *
 * Allocates and initializes a new child module entry with a hierarchy level
 * incremented relative to its parent, sets the parent link, and appends it to
 * the parent's sub-modules list.
 *
 * @param[in] parent_mod Pointer to the parent module_t structure. Must not be
 * NULL.
 * @param[in] name       Name of the sub-module. Must not be NULL.
 * @param[in] usage      Usage syntax string, or NULL if none.
 * @param[in] options    Array of option structure pointers, or NULL if none.
 * @param[in] opt_nr     Total number of options in the array.
 *
 * @return Pointer to the embedded public module_t structure of the child, or
 * NULL on failure.
 */
module_t *add_submodule(
    module_t *parent_mod,
    const char *name,
    const char *usage,
    module_option_t **options,
    unsigned int opt_nr);

/**
 * @brief Parses command-line arguments, activates matching modules, and
 * captures options.
 *
 * @param[in] argv Array of command-line argument strings (NULL-terminated).
 *
 * @return 0 on success, -1 on error
 */
int launch_cli(char **argv);

module_t *set_root_module(
    const char *name,
    const char *usage,
    module_option_t **options,
    unsigned int opt_nr);

#endif /* INCLUDE_MODULE_H */