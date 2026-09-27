// SPDX-License-Identifier: GPL-2.0
//
// vim: set ts=8 sw=8 noet tw=80 cc=80 fo+=t :

#ifndef INCLUDE_MODULE_H
#define INCLUDE_MODULE_H

#include <getopt.h>
#include <stdbool.h>
#include <stddef.h>

#include "debug.h"
#include "lists.h"

/**
 * @def CLI_IGNORE_MODULE_ERRORS
 * @brief Command chaining support:
 * If the requested module is not found within the current submodule scope,
 * fallback to 'root_sub_modules' to allow execution of top-level commands
 * in a single invocation (e.g., kfgx net -i eth0 disk status).
 */
#define CLI_ENABLE_MODULES_CHAINING

/**
 * @def CLI_IGNORE_MODULE_ERRORS
 * @brief Controls error tolerance during chained module execution.
 *
 * When defined, the CLI engine continues executing remaining modules in the
 * sequence
 * (`dispatch_cli`) even if a previous module's `action` callback returns an
 * error code (non-zero value).
 *
 * @note By default (macro undefined), any error returned by `action`
 * immediately halts execution and stops remaining modules from running.
 *
 * @warning Errors are still logged via \c pr_error / \c pr_warn, but the final
 * return code may conceal intermediate step failures.
 */
#define CLI_IGNORE_MODULE_ERRORS

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
    /**
     * @brief Module execution handler invoked after option parsing.
     *
     * @param[in] optc Number of active/matched options.
     * @param[in] optv Array of pointers to active options.
     *
     * @return 0 on success, non-zero on failure.
     *
     * @warning **HEAP ALLOCATION & MEMORY OWNERSHIP**
     * @par
     * The \p optv array is dynamically allocated on the **HEAP** by the
     * KernForgeX engine.
     *
     * **Strict Developer Rules:**
     * 1. **DO NOT CALL \c free() ON \p optv OR ITS ELEMENTS:** Memory lifecycle
     *    is entirely managed by the framework. Calling \c free() inside this
     * callback will result in *Double-Free* or *Use-After-Free* heap
     * corruption.
     * 2. **DO NOT MUTATE POINTERS OR STRINGS IN-PLACE:** Do not alter internal
     * option fields.
     * 3. **ALWAYS MAKE A DEEP COPY FOR PERSISTENCE:** If option values are
     * required beyond the callback scope, you **MUST** create a deep copy into
     * your module's private context.
     *
     * @note Mandatory as of this revision: add_module(), add_submodule() and
     * set_root_module() all now take `action` as a required registration
     * parameter and reject (return NULL for) any attempt to register a
     * module without one. This is enforced because cli_parsing() calls
     * m->action() unconditionally for any matched module that captured at
     * least one argument - an unset handler previously meant a NULL
     * function-pointer call discovered only at CLI-dispatch time, instead
     * of a clear failure at registration time.
     */
    int (*action)(int optc, module_option_t **optv);
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
 * @param[in] action  Handler invoked after option parsing for this module.
 * Mandatory: must not be NULL, or registration fails (see module_t::action).
 *
 * @return Pointer to the embedded public module_t structure, or NULL on
 * failure.
 */
module_t *add_module(
    const char *name,
    const char *usage,
    module_option_t **options,
    unsigned int opt_nr,
    int (*action)(int optc, module_option_t **optv));

/**
 * @brief Allocates or updates the standalone global root module for top-level
 * options.
 *
 * @param[in] name    Name of the application/root entry. Must not be NULL.
 * @param[in] usage   Usage syntax pattern (e.g. "[options] <command>"), or
 * NULL.
 * @param[in] options Array of option structure pointers, or NULL if none.
 * @param[in] opt_nr  Total number of options in the array.
 * @param[in] action  Handler invoked after option parsing for the root
 * module. Mandatory: must not be NULL, or the call fails (see
 * module_t::action).
 *
 * @return Pointer to the embedded public module_t structure, or NULL on
 * failure.
 */
module_t *set_root_module(
    const char *name,
    const char *usage,
    module_option_t **options,
    unsigned int opt_nr,
    int (*action)(int optc, module_option_t **optv));

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
 * @param[in] action     Handler invoked after option parsing for this
 * sub-module. Mandatory: must not be NULL, or registration fails (see
 * module_t::action).
 *
 * @return Pointer to the embedded public module_t structure of the child, or
 * NULL on failure.
 */
module_t *add_submodule(
    module_t *parent_mod,
    const char *name,
    const char *usage,
    module_option_t **options,
    unsigned int opt_nr,
    int (*action)(int optc, module_option_t **optv));

/**
 * @brief Parses command-line arguments, activates matching modules, and
 * captures options.
 *
 * @param[in] argv Array of command-line argument strings (NULL-terminated).
 *
 * @return 0 on success, -1 on error
 */
int launch_cli(char *const *argv);

/**
 * @brief Renders CLI usage instructions for a module.
 *
 * @param[in] stream Output file stream (defaults to stderr if NULL).
 * @param[in] prog_name Executable name or path
 * @param[in] m Pointer to the target module structure containing options and
 * metadata.
 *
 * @return 0 on success, -1 if the module pointer is invalid.
 */
int pr_module_usage(FILE *stream, const char *prog_name, const module_t *m);

/**
 * @brief Prints the summary usage for a module.
 *
 * @param[in] stream Output file stream (defaults to stderr if NULL).
 * @param[in] progr_name Executable name or path .
 * @param[in] m Pointer to the target module structure.
 */
void pr_module_short_usage(
    FILE *stream, const char *progr_name, const module_t *m);

#endif /* INCLUDE_MODULE_H */