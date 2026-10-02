// SPDX-License-Identifier: GPL-2.0
//
// vim: set ts=8 sw=8 noet tw=80 cc=80 fo+=t :

#ifndef INCLUDE_GENERIC_MODULE_H
#define INCLUDE_GENERIC_MODULE_H

#include <getopt.h>
#include <stdbool.h>
#include <stddef.h>

#include "generic/lists.h"

#ifndef DFLT_PROG_NAME
#define DFLT_PROG_NAME "program"
#endif

/**
 * @struct module_option_struct
 * @brief Represents a command-line option associated with a module.
 */
typedef struct module_option_struct {
    char s_opt;        /**< Short single-character flag */
    const char *l_opt; /**< Long string flag */
    int has_arg;       /**< Requirement level for option arguments */
    const char
        *arg_name; /**< Descriptive label for option arguments in usage text */
    const char *desc; /**< Explanatory help text describing the option */

    bool
        is_set; /**< Runtime flag indicating if the option was matched on CLI */
    const char *arg_val; /**< Pointer to the captured argument value string */
} module_option_t;

/**
 * @struct module_struct
 * @brief Public interface structure representing a registered module and its
 * options.
 */
typedef struct module_struct {
    char *name; /**< Heap-allocated name identifying the module */
    module_option_t *
        *options; /**< Array of pointers to the module's supported options */
    unsigned int opt_nr; /**< Total count of options in the array */
    const char *usage;   /**< Usage syntax template pattern */

    /**
     * @brief Custom callback handler invoked after options are parsed and
     * matched.
     *
     * @param[in] optc Number of active options successfully matched on the
     * command line.
     * @param[in] optv Array of pointers to the matched active options.
     *
     * @return Zero on successful execution, or a negative/non-zero error code
     * on failure.
     *
     * @warning **CRITICAL MEMORY OWNERSHIP RULES:**
     * - The `optv` array is dynamically allocated by the framework. Never call
     * `free()` on `optv` or its elements inside your handler to prevent heap
     * corruption.
     * - Do not modify option fields directly in-place.
     * - If option values must persist beyond the lifespan of this callback, you
     * are required to make a deep copy into your own module-specific storage.
     *
     * @note **CONCURRENCY && THREAD-SAFETY:** The internal registry lock is
     * safely released while this handler runs. This allows your callback to
     * safely register new modules or submodules on the fly without risking
     * deadlocks.
     */
    int (*action)(int optc, module_option_t **optv);
} module_t;

/**
 * @brief Registers a new top-level (Level 0) module in the global hierarchy.
 *
 * @param[in] name    Name identifying the module. Must not be NULL.
 * @param[in] usage   Usage syntax pattern string, or NULL if none.
 * @param[in] options Array of option pointers, or NULL if none.
 * @param[in] opt_nr  Total number of options provided in the array.
 * @param[in] action  Execution handler callback. Mandatory and must not be
 * NULL.
 *
 * @return Pointer to the embedded public module structure, or NULL on failure.
 *
 * @note **CONCURRENCY && THREAD-SAFETY:**
 * Safe to call concurrently from multiple threads. Memory allocation and
 * deep-copying are performed lock-free prior to entry; only the final link
 * step into `root_sub_modules` is serialized under `module_ctx.mutex`.
 */
module_t *register_module(
    const char *name,
    const char *usage,
    module_option_t options[],
    unsigned int opt_nr,
    int (*action)(int optc, module_option_t **optv));

/**
 * @brief Creates or replaces the standalone global root module.
 *
 * @param[in] name    Name for the application root context. Must not be NULL.
 * @param[in] usage   Usage syntax pattern string, or NULL if none.
 * @param[in] options Array of option pointers, or NULL if none.
 * @param[in] opt_nr  Total number of options provided in the array.
 * @param[in] action  Execution handler callback. Mandatory and must not be
 * NULL.
 *
 * @return Pointer to the embedded public module structure, or NULL on failure.
 *
 * @note **CONCURRENCY && THREAD-SAFETY:**
 * Fully protected under `module_ctx.mutex`. Concurrent calls are serialized
 * via a last-write-wins swap strategy. The new subtree is built off-lock
 * before atomically retiring and freeing the previous instance.
 */
module_t *set_root_module(
    const char *name,
    const char *usage,
    module_option_t options[],
    unsigned int opt_nr,
    int (*action)(int optc, module_option_t **optv));

/**
 * @brief Registers a nested sub-module underneath an existing parent module.
 *
 * @param[in] parent_mod Pointer to the parent module. Must not be NULL.
 * @param[in] name       Name identifying the submodule. Must not be NULL.
 * @param[in] usage      Usage syntax pattern string, or NULL if none.
 * @param[in] options    Array of option pointers, or NULL if none.
 * @param[in] opt_nr     Total number of options provided in the array.
 * @param[in] action     Execution handler callback. Mandatory and must not be
 * NULL.
 *
 * @return Pointer to the embedded public module structure of the child, or NULL
 * on failure.
 *
 * @note **CONCURRENCY && THREAD-SAFETY:**
 * Safe for concurrent invocation. Parent hierarchy level is read lock-free
 * (immutable post-creation), and tree insertion is guarded by
 * `module_ctx.mutex`.
 * @warning The caller must guarantee that `parent_mod` remains valid during the
 * call.
 */
module_t *register_submodule(
    module_t *parent_mod,
    const char *name,
    const char *usage,
    module_option_t options[],
    unsigned int opt_nr,
    int (*action)(int optc, module_option_t **optv));

/**
 * @brief Executes the complete CLI pipeline: parsing, argument matching, and
 * action dispatch.
 *
 * @param[in] argv NULL-terminated array of command-line argument strings.
 * @return Zero on complete success, or a negative error code on failure.
 *
 * @note **CONCURRENCY && REENTRANCY:**
 * Enforces strict single-flight execution via an atomic compare-exchange on
 * `module_ctx.cli_running`. Concurrent or re-entrant execution attempts are
 * rejected immediately.
 */
int launch_cli(char *const *argv);

/**
 * @brief Renders full, detailed usage instructions for a specific module.
 *
 * @param[in] stream   Target output file stream for rendering text.
 * @param[in] prog_name Executable program name or invocation path.
 * @param[in] m        Pointer to the target module structure.
 * @return Zero on success, or -1 if parameters are invalid.
 */
int pr_module_usage(FILE *stream, const module_t *m);

/**
 * @brief Prints a concise, short summary usage line for a module.
 *
 * @param[in] stream   Target output file stream for rendering text.
 * @param[in] prog_name Executable program name or invocation path.
 * @param[in] m        Pointer to the target module structure.
 */
void pr_module_short_usage(FILE *stream, const module_t *m);

/**
 * @brief Writes a POSIX sh completion script covering the whole registry.
 *
 * @param[in] output_path Path to write (e.g. "sh_completions.sh").
 *
 * @return 0 on success, -1 on invalid argument or I/O failure.
 *
 * @note Thread-safe. Call after registering every module, before
 * launch_cli() (which destroys the registry).
 */
int generate_sh_completion(const char *output_path);

/**
 * @brief Writes an idiomatic bash completion script (arrays, `local`,
 * `compgen`, `complete -F`) covering the whole registry.
 *
 * @param[in] output_path Path to write (e.g. "bash_completions.sh").
 *
 * @return 0 on success, -1 on invalid argument or I/O failure.
 *
 * @note Thread-safe. Call after registering every module, before
 * launch_cli() (which destroys the registry).
 */
int generate_bash_completion(const char *output_path);

/**
 * @brief Stores the executable name in the registry instance.
 */
void module_set_prog_name(const char *prog_name);

#endif /* INCLUDE_GENERIC_MODULE_H */