// SPDX-License-Identifier: GPL-2.0
//
// vim: set ts=8 sw=8 noet tw=80 cc=80 fo+=t :

/**
 * @file      modules.c
 * @author    midnight walker
 * @brief     Hierarchical CLI module registry, argument matching and handler
 *            dispatch.
 * @version   0.4
 * @date      2026-09-25
 * @copyright GNU General Public License v2.0
 *
 */

#define prfx_fmt "modules: "

#include <ctype.h>
#include <getopt.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "modules.h"

/**
 * @struct module_entry_struct
 * @brief Private internal container used for hierarchical tree management and
 * parsing state.
 */
typedef struct module_entry_struct {
    module_t mod; /**< Embedded public module structure */

    char **args; /**< Dynamic array holding raw arguments captured from argv */
    unsigned int args_nr; /**< Total number of captured argument tokens */

    bool matched; /**< Internal state flag tracking activation during parsing */
    unsigned int
        level; /**< Depth level of the module within the tree (0 is root) */

    struct module_entry_struct
        *parent; /**< Pointer to parent module entry, or NULL if top-level */
    list_node_t sub_modules; /**< Intrusive list head for child submodules */
    list_node_t
        siblings; /**< Intrusive list node linking modules at the same level */
    list_node_t
        matches; /**< Intrusive list node chaining modules activated in order */
} module_entry_t;

/**
 * @brief Converts a public module pointer to its parent internal entry
 * container.
 *
 * @param[in] user_mod_ptr Pointer to the public module_t structure.
 * @return Pointer to the enclosing module_entry_t structure.
 */
#define to_module_entry(user_mod_ptr)                                          \
    container_of(user_mod_ptr, module_entry_t, mod)

/**
 * @struct module_ctx_struct
 * @brief Global state of the module registry.
 */
struct module_ctx_struct {
    /**
     * Root module holding the options given before any sub-command.
     * NULL until set_root_module() succeeds.
     */
    module_entry_t *root_entry;

    /**
     * Head of the circular list of top-level (level 0) modules registered
     * with register_module(). Also the scope searched first by the parser.
     */
    list_node_t root_sub_modules;

    /**
     * Head of the list of modules activated during argument parsing, in
     * command-line order. Linked through module_entry_t::matches.
     */
    list_node_t matched_mods;

    _Atomic(const char *)
        prog_name; /**< Program name or invocation path, from argv[0] */

    pthread_mutex_t mutex;

    atomic_bool cli_running;
};

/**
 * @brief The single registry instance.
 */
static struct module_ctx_struct module_ctx = {
    .root_entry = NULL,
    .root_sub_modules =
        {&module_ctx.root_sub_modules, &module_ctx.root_sub_modules},
    .matched_mods = {&module_ctx.matched_mods, &module_ctx.matched_mods},
    .prog_name = NULL,
    .cli_running = false,
};

static pthread_once_t module_mutex_once = PTHREAD_ONCE_INIT;

static void init_module_mutex_attr(void)
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&module_ctx.mutex, &attr);
    pthread_mutexattr_destroy(&attr);
}

static inline void module_mutex_lock(void)
{
    pthread_once(&module_mutex_once, init_module_mutex_attr);
    pthread_mutex_lock(&module_ctx.mutex);
}

static inline void module_mutex_unlock(void)
{
    pthread_mutex_unlock(&module_ctx.mutex);
}

/**
 * @brief Stores the executable name in the registry instance.
 */
static void module_store_prog_name_locked(const char *prog_name)
{
    atomic_store(&module_ctx.prog_name, prog_name);
}

/**
 * @brief Stores the executable name in the registry instance.
 */
void module_set_prog_name(const char *prog_name)
{
    atomic_store(&module_ctx.prog_name, prog_name);
}

/**
 * @brief Reads the executable name from the registry instance.
 *
 * Thread-safe and lock-free: reads an atomic pointer without taking
 * module_ctx.mutex, preventing deadlocks when called from usage rendering.
 */
const char *module_get_prog_name(void)
{
    const char *name = atomic_load(&module_ctx.prog_name);
    return name ? name : DFLT_PROG_NAME;
}

static void free_module_tree(list_node_t *head);

/**
 * @brief Releases every heap allocation owned by a public module_t.
 *
 * @note **CONCURRENCY && THREAD-SAFETY:** Not thread-safe by itself. Callers
 * touching a module_t reachable from module_ctx must hold module_ctx.mutex.
 *
 * @param[in,out] mod Module to release. Its storage itself is not freed.
 */
static void free_module_public(module_t *mod)
{
    if (mod->options) {
        for (unsigned int i = 0; i < mod->opt_nr; i++) {
            module_option_t *opt = mod->options[i];

            if (!opt)
                continue;

            free((char *)opt->l_opt);
            free((char *)opt->arg_name);
            free((char *)opt->desc);
            free(opt);
        }
        free(mod->options);
    }

    free(mod->name);
    free((char *)mod->usage);

    memset(mod, 0, sizeof(*mod));
}

/**
 * @brief Deep-copies one option definition.
 *
 * @param[out] dst Receives the newly allocated option.
 * @param[in]  src Option definition to copy.
 *
 * @return 0 on success, -1 on allocation failure.
 */
static int dup_option(module_option_t **dst, const module_option_t *src)
{
    module_option_t *opt = malloc(sizeof(*opt));
    if (!opt)
        return -1;

    memcpy(opt, src, sizeof(*opt));

    /* Drop the borrowed pointers copied by memcpy(); own copies follow. */
    opt->l_opt = NULL;
    opt->arg_name = NULL;
    opt->desc = NULL;
    *dst = opt;

    if (src->l_opt && !(opt->l_opt = strdup(src->l_opt)))
        return -1;
    if (src->arg_name && !(opt->arg_name = strdup(src->arg_name)))
        return -1;
    if (src->desc && !(opt->desc = strdup(src->desc)))
        return -1;

    return 0;
}

/**
 * @brief Builds the public part of a module from caller-supplied data.
 *
 * On failure nothing is leaked and @p mod is left zeroed.
 *
 * @param[out] mod     Module to initialize (any previous content is ignored).
 * @param[in]  name    Module name. Must not be NULL.
 * @param[in]  usage   Usage syntax pattern, or NULL if none.
 * @param[in]  options Array of option pointers, or NULL if none.
 * @param[in]  opt_nr  Number of options in @p options.
 * @param[in]  action  Execution handler. Must not be NULL; callers reject a
 *                     NULL handler before reaching this point.
 *
 * @return 0 on success, -1 on allocation failure.
 */
static int init_module_public(
    module_t *mod,
    const char *name,
    const char *usage,
    module_option_t options[],
    unsigned int opt_nr,
    int (*action)(int optc, module_option_t **optv))
{
    memset(mod, 0, sizeof(*mod));

    mod->name = strdup(name);
    if (!mod->name)
        goto err;

    if (usage) {
        mod->usage = strdup(usage);
        if (!mod->usage)
            goto err;
    }

    if (options && opt_nr > 0) {
        mod->options = calloc(opt_nr, sizeof(*mod->options));
        if (!mod->options)
            goto err;
        mod->opt_nr = opt_nr;

        for (unsigned int i = 0; i < opt_nr; i++) {
            if (dup_option(&mod->options[i], &options[i]) != 0)
                goto err;
        }
    }

    mod->action = action;
    return 0;

err:
    free_module_public(mod);
    return -1;
}

/**
 * @brief Allocates and initializes a new module entry.
 *
 * @param[in] name    Module name. Must not be NULL.
 * @param[in] usage   Usage syntax pattern, or NULL if none.
 * @param[in] options Array of option pointers, or NULL if none.
 * @param[in] opt_nr  Number of options in @p options.
 * @param[in] action  Execution handler. Must not be NULL.
 * @param[in] level   Depth of the module in the hierarchy (0 = top level).
 *
 * @return The new entry, or NULL on allocation failure.
 */
static module_entry_t *create_module_entry(
    const char *name,
    const char *usage,
    module_option_t options[],
    unsigned int opt_nr,
    int (*action)(int optc, module_option_t **optv),
    unsigned int level)
{
    module_entry_t *entry = calloc(1, sizeof(*entry));
    if (!entry)
        return NULL;

    if (init_module_public(&entry->mod, name, usage, options, opt_nr, action) !=
        0) {
        free(entry);
        return NULL;
    }

    entry->level = level;
    list_init(&entry->sub_modules);
    list_init(&entry->siblings);
    list_init(&entry->matches);

    return entry;
}

/**
 * @brief Destroys a module entry together with its whole sub-tree.
 *
 * @note **CONCURRENCY && THREAD-SAFETY:** Not thread-safe by itself. The caller
 * must hold module_ctx.mutex, since this unlinks the entry from lists
 * module_ctx owns (matched_mods, and the parent's sibling list) and recurses
 * into its sub-tree.
 *
 * @param[in] entry Entry to destroy. Invalid after the call.
 */
static void destroy_module_entry(module_entry_t *entry)
{
    free_module_tree(&entry->sub_modules);

    list_del(&entry->matches);
    list_del(&entry->siblings);

    free_module_public(&entry->mod);
    free(entry->args);
    free(entry);
}

/**
 * @brief Recursively destroys every module linked in a sub-modules list.
 *
 * @note **CONCURRENCY && THREAD-SAFETY:** Not thread-safe by itself. The caller
 * must hold module_ctx.mutex (see destroy_module_entry()).
 *
 * @param[in] head Head of the list to empty. Left as a valid empty list, so
 * it can safely be reused or emptied again.
 */
static void free_module_tree(list_node_t *head)
{
    if (!head)
        return;

    list_node_t *curr = head->next;
    while (curr != head) {
        /* Saved first: destroying the entry unlinks and frees `curr`. */
        list_node_t *next = curr->next;

        destroy_module_entry(container_of(curr, module_entry_t, siblings));
        curr = next;
    }
}

/**
 * @brief Destroys the global root module, if any.
 *
 * @note **CONCURRENCY && THREAD-SAFETY:** Not thread-safe by itself. The caller
 * must hold module_ctx.mutex.
 */
static void free_root_entry(void)
{
    if (!module_ctx.root_entry)
        return;

    destroy_module_entry(module_ctx.root_entry);
    module_ctx.root_entry = NULL;
}

/**
 * @brief Creates or replaces the global root module.
 *
 * On first call the root entry is allocated. On later calls it is
 * re-initialized in place, so the returned pointer stays the same. Any
 * sub-modules attached to the previous root and any arguments it captured
 * are discarded.
 *
 * Re-initialization is atomic: the new configuration is built before the old
 * one is touched, so on failure the previous root module is left unchanged.
 *
 * @param[in] name    Application/root name. Must not be NULL.
 * @param[in] usage   Usage syntax pattern, or NULL.
 * @param[in] options Array of option pointers, or NULL if none.
 * @param[in] opt_nr  Number of options in @p options.
 * @param[in] action  Execution handler for the root module. Mandatory.
 *
 * @return The root's public module_t, or NULL on invalid argument or
 * allocation failure.
 *
 * @note **CONCURRENCY && THREAD-SAFETY:** Fully protected under
 * `module_ctx.mutex`. The whole check-then-act sequence runs as a single
 * critical section.
 */
module_t *set_root_module(
    const char *name,
    const char *usage,
    module_option_t options[],
    unsigned int opt_nr,
    int (*action)(int optc, module_option_t **optv))
{
    if (!name || !action)
        return NULL;

    module_mutex_lock();

    module_entry_t *root = module_ctx.root_entry;

    if (!root) {
        root = create_module_entry(name, usage, options, opt_nr, action, 0);
        if (root) {
            module_ctx.root_entry = root;
            pr_debug(
                "Created new root module '%s' with %u options", name, opt_nr);
        } else {
            pr_error("Failed to create root module '%s'", name);
        }
        module_mutex_unlock();
        return root ? &root->mod : NULL;
    }

    module_t fresh;
    if (init_module_public(&fresh, name, usage, options, opt_nr, action) != 0) {
        module_mutex_unlock();
        return NULL;
    }

    /* The new configuration is ready: retire the old one and swap. */
    free_module_tree(&root->sub_modules);
    free_module_public(&root->mod);
    free(root->args);
    root->args = NULL;
    root->args_nr = 0;

    root->mod = fresh;
    root->level = 0;
    root->matched = false;

    list_del(&root->matches);
    list_init(&root->sub_modules);
    list_init(&root->siblings);
    list_init(&root->matches);

    module_mutex_unlock();

    pr_debug("Re-initialized root module '%s' with %u options", name, opt_nr);

    return &root->mod;
}

/**
 * @brief Registers a top-level (level 0) module.
 *
 * @param[in] name    Module name. Must not be NULL.
 * @param[in] usage   Usage syntax pattern, or NULL.
 * @param[in] options Array of option pointers, or NULL if none.
 * @param[in] opt_nr  Number of options in @p options.
 * @param[in] action  Execution handler. Mandatory.
 *
 * @return The module's public module_t, or NULL on invalid argument or
 * allocation failure.
 *
 * @note **CONCURRENCY && THREAD-SAFETY:** Safe to call concurrently from
 * multiple threads. The entry is built lock-free prior to entry; only linking
 * it into root_sub_modules is serialized under `module_ctx.mutex`.
 */
module_t *register_module(
    const char *name,
    const char *usage,
    module_option_t options[],
    unsigned int opt_nr,
    int (*action)(int optc, module_option_t **optv))
{
    if (!name || !action)
        return NULL;

    module_entry_t *entry =
        create_module_entry(name, usage, options, opt_nr, action, 0);
    if (!entry)
        return NULL;

    module_mutex_lock();
    list_add_tail(&module_ctx.root_sub_modules, &entry->siblings);
    module_mutex_unlock();

    return &entry->mod;
}

/**
 * @brief Registers a sub-module under an existing module.
 *
 * The child's level is its parent's level plus one.
 *
 * @param[in] parent_mod Parent module. Must not be NULL.
 * @param[in] name       Sub-module name. Must not be NULL.
 * @param[in] usage      Usage syntax pattern, or NULL.
 * @param[in] options    Array of option pointers, or NULL if none.
 * @param[in] opt_nr     Number of options in @p options.
 * @param[in] action     Execution handler. Mandatory.
 *
 * @return The sub-module's public module_t, or NULL on invalid argument or
 * allocation failure.
 *
 * @note **CONCURRENCY && THREAD-SAFETY:** Safe for concurrent invocation.
 * `parent_entry->level` is read lock-free as it is immutable post-creation, and
 * tree insertion is guarded by `module_ctx.mutex`.
 * @warning The caller must guarantee that @p parent_mod remains valid
 * throughout the call.
 */
module_t *register_submodule(
    module_t *parent_mod,
    const char *name,
    const char *usage,
    module_option_t options[],
    unsigned int opt_nr,
    int (*action)(int optc, module_option_t **optv))
{
    if (!parent_mod || !name || !action)
        return NULL;

    module_entry_t *parent_entry = to_module_entry(parent_mod);

    module_entry_t *child = create_module_entry(
        name, usage, options, opt_nr, action, parent_entry->level + 1);
    if (!child)
        return NULL;

    child->parent = parent_entry;

    module_mutex_lock();
    list_add_tail(&parent_entry->sub_modules, &child->siblings);
    module_mutex_unlock();

    return &child->mod;
}

/**
 * @brief Looks up a module by name in one sub-modules list.
 *
 * @note **CONCURRENCY && THREAD-SAFETY:** Not thread-safe by itself. The caller
 * must hold module_ctx.mutex.
 *
 * @param[in] sub_modules_head Head of the list to search.
 * @param[in] name             Module name to find.
 *
 * @return The matching entry, or NULL if there is none.
 */
static module_entry_t *
search_module_by_name(list_node_t *sub_modules_head, const char *name)
{
    if (!sub_modules_head || !name)
        return NULL;

    list_node_t *curr;
    list_for_each(curr, sub_modules_head)
    {
        module_entry_t *entry = container_of(curr, module_entry_t, siblings);
        if (entry->mod.name && strcmp(entry->mod.name, name) == 0)
            return entry;
    }
    return NULL;
}

/**
 * @brief Releases every module resource created for this CLI invocation.
 *
 * Must run only once matching and dispatch are both finished: destroying the
 * modules empties matched_mods, which cli_parsing() still needs to walk.
 *
 * @note **CONCURRENCY && THREAD-SAFETY:** Takes `module_ctx.mutex` internally
 * to prevent races with concurrent registration routines on other threads.
 */
static void cleanup_cli_modules(void)
{
    module_mutex_lock();
    free_module_tree(&module_ctx.root_sub_modules);
    free_root_entry();
    module_mutex_unlock();
}

/**
 * @brief Matches argv against the module tree and captures option tokens.
 *
 * Tokens starting with '-' are appended to the args array of the currently
 * active module (the root module until a module name is matched). Any other
 * token must be the name of a module in the current scope, which then becomes
 * the active module. With CLI_ENABLE_MODULES_CHAINING, a name not found in the
 * current scope is also searched among the top-level modules.
 *
 * Each matched module is appended to matched_mods. The root module is added
 * lazily, the first time it captures an option, so it only appears there when
 * it actually has something to process.
 *
 * @note Frees nothing: matched modules and their captured tokens must remain
 * valid for cli_parsing(). The caller must run cleanup_cli_modules() when
 * done (see parse_cli()).
 *
 * @note **CONCURRENCY && THREAD-SAFETY:** Holds `module_ctx.mutex` for its
 * entire run (no user code is invoked here), serializing access against
 * concurrent registrations.
 *
 * @param[in] argv NULL-terminated argument vector; argv[0] is skipped.
 *
 * @return 0 on success, -1 on error.
 */
static int parse_cli_modules(char *const *argv)
{
    if (!argv || !*argv)
        return 0;

    int ret = -1;

    module_mutex_lock();

    /* Store the program name for later use in error and usage messages. */
    module_store_prog_name_locked(argv[0]);

    list_node_t *current_sub_modules = &module_ctx.root_sub_modules;
    module_entry_t *current_mod = module_ctx.root_entry;

    for (char *const *args = argv + 1; *args != NULL; args++) {
        const char *arg = *args;

        /* Option token: attach it to the active module. */
        if (arg[0] == '-') {
        make_arg:

            if (!current_mod) {
                pr_error("No module active to handle option '%s'", arg);
                goto out;
            }

            if (current_mod == module_ctx.root_entry && !current_mod->matched) {
                current_mod->matched = true;
                list_add_tail(&module_ctx.matched_mods, &current_mod->matches);
            }

            char **new_args = realloc(
                current_mod->args, (current_mod->args_nr + 1) * sizeof(char *));
            if (!new_args) {
                pr_error("Failed to create an argument");
                goto out;
            }
            current_mod->args = new_args;
            current_mod->args[current_mod->args_nr++] = *args;

            continue;
        }
        /* Module name: descend into it. */
        module_entry_t *found = search_module_by_name(current_sub_modules, arg);
#ifdef CLI_ENABLE_MODULES_CHAINING
        if (!found && current_sub_modules != &module_ctx.root_sub_modules) {
            current_sub_modules = &module_ctx.root_sub_modules;
            found = search_module_by_name(current_sub_modules, arg);
        }
#endif
        /* if is not a module name, treat it as an argument for the current
         * module */
        if (!found) {
            pr_debug(
                "Module or submodule '%s' unknown in this context, treating as "
                "argument",
                arg);
            goto make_arg;
        }

        if (found->matched) {
            pr_debug(
                "Module '%s' has already been specified, treating as argument",
                arg);
            goto make_arg;
        }

#ifdef CLI_ENABLE_MODULES_LINEAR_TREE_CHAINING
        if (current_mod->args)
            goto make_arg;
#endif

        found->matched = true;
        current_mod = found;

        list_add_tail(&module_ctx.matched_mods, &found->matches);
        current_sub_modules = &found->sub_modules;
    }

    pr_info("Loading specified modules completed successfully");

#ifdef DEBUG
    list_node_t *curr_node;
    list_for_each(curr_node, &module_ctx.matched_mods)
    {
        module_entry_t *e = container_of(curr_node, module_entry_t, matches);
        pr_debug(
            "Module: <%s> [level='%u', '%u' argument(s)]",
            e->mod.name,
            e->level,
            e->args_nr);
        for (unsigned int i = 0; i < e->args_nr; i++)
            pr_debug("[ args[%u] = %s ]", i + 1, e->args[i]);
    }
#endif

    ret = 0;

out:
    if (ret != 0)
        pr_error_to_user(
            "Failed to parse the command line arguments for modules");
    module_mutex_unlock();
    return ret;
}

/** Returns @p ch if printable, '#' otherwise (safe to show in diagnostics). */
#define printable(ch) (isprint((unsigned char)(ch)) ? (ch) : '#')

/**
 * @brief Runs getopt_long() over the tokens captured for one module.
 *
 * Every recognized option gets is_set = true and arg_val = its argument.
 * Unknown options, missing arguments and unmapped codes are reported on
 * stderr (with the module's usage line when available) and abort the parse.
 *
 * @param[in] argc         Number of captured tokens.
 * @param[in] argv         Captured tokens (without a program name).
 * @param[in] optstring    getopt_long() short-option string.
 * @param[in] long_options getopt_long() long-option table, zero terminated.
 * @param[in] m            Module owning the options. Must not be NULL.
 *
 * @return 0 on success, 1 if --help/-h was shown, -1 on error.
 *
 * @note **CONCURRENCY && THREAD-SAFETY:** Not thread-safe by itself. Relies on
 * process-global getopt state (`optind`/`optarg`/etc.). The caller
 * (`cli_parsing()`) must hold `module_ctx.mutex` to safely serialize
 * executions.
 */
static int parse_cli_modules_options(
    int argc,
    char *const *argv,
    const char *optstring,
    const struct option *long_options,
    module_t *m)
{
    if (!m)
        return -1;

    if (!argv || argc <= 0)
        return 0;

    /* getopt_long() expects argv[0] to be a program name: use the module's. */
    char **parse_argv = malloc((size_t)(argc + 1) * sizeof(char *));
    if (!parse_argv) {
        pr_error("Allocation memory failed");
        return -1;
    }

    parse_argv[0] = m->name;
    for (int i = 0; i < argc; i++)
        parse_argv[i + 1] = argv[i];
    int parse_argc = argc + 1;

    /* Full reset of getopt's internal state (glibc-specific optind = 0). */
    optind = 0;
    /* Silence getopt's own messages: errors are reported below. */
    opterr = 0;

    int opt;
    int long_idx = -1;
    int ret = 0;

    while ((opt = getopt_long(
                parse_argc, parse_argv, optstring, long_options, &long_idx)) !=
           -1) {
        pr_debug(
            "parsed opt=%d ('%c'), optind=%d", opt, printable(opt), optind);

        if (opt == '?') {
            const char *last_arg = (optind > 0 && optind <= parse_argc)
                                       ? parse_argv[optind - 1]
                                       : NULL;

            if (optopt != 0) {
                pr_error_to_user(
                    "Unrecognized option: '-%c'\n", printable(optopt));
            } else if (last_arg) {
                pr_error_to_user("Unrecognized option: '%s'\n", last_arg);
            } else {
                pr_error_to_user("Unrecognized option\n");
            }
            if (m->usage)
                pr_module_usage(stderr, m);
            ret = -1;
            goto out;
        }

        if (opt == ':') {
            pr_error_to_user(
                "Option requires an argument: '-%c'\n", printable(optopt));
            if (m->usage)
                pr_module_usage(stderr, m);
            ret = -1;
            goto out;
        }

        /*
         * Map the getopt result back to the module's option. A long option
         * reports its index in long_options, which mirrors m->options;
         * a short option is found by its character.
         */
        module_option_t *matched = NULL;

        if (long_idx >= 0 && (size_t)long_idx < m->opt_nr) {
            matched = m->options[long_idx];
        } else {
            for (size_t i = 0; i < m->opt_nr; i++) {
                if (m->options[i]->s_opt &&
                    (int)(unsigned char)m->options[i]->s_opt == opt) {
                    matched = m->options[i];
                    break;
                }
            }
        }

        if (!matched) {
            pr_error_to_user("unhandled option code: %d\n", opt);
            pr_module_usage(stderr, m);
            ret = -1;
            goto out;
        }

        matched->is_set = true;
        matched->arg_val = optarg;
        pr_debug(
            "matched option '--%s' (-%c), val='%s'",
            matched->l_opt ? matched->l_opt : "",
            matched->s_opt ? matched->s_opt : ' ',
            matched->arg_val ? matched->arg_val : "none");

        /* getopt_long() only updates long_idx for long options. */
        long_idx = -1;
    }

    if (optind < parse_argc) {
        pr_error_to_user(
            "Unexpected trailing argument or unknown subcommand: '%s'\n",
            parse_argv[optind]);
        if (m->usage)
            pr_module_usage(stderr, m);
        ret = -1;
        goto out;
    }

    pr_debug(
        "Parsing completed successfully for module: %s",
        m->name ? m->name : "unknown");

out:
    free(parse_argv);
    return ret;
}

/**
 * @brief Parses the captured options of every matched module and runs its
 * handler.
 *
 * For each matched module that has both registered options and captured
 * tokens, this builds the getopt_long() tables from its option definitions,
 * parses the tokens, then calls the module's action with only the options
 * that were set. That array is owned by the framework and released right
 * after the call, as documented for module_t::action.
 *
 * A failing handler stops the run, unless CLI_IGNORE_MODULE_ERRORS is
 * defined, in which case the next module is processed.
 *
 * @note **CONCURRENCY && THREAD-SAFETY:** `module_ctx.mutex` is held globally,
 * but temporarily released right before invoking a module's `action()`
 * callback, and re-acquired immediately after it returns.
 *
 * @return 0 on success, -1 on error.
 */
static int cli_parsing(void)
{
    int ret = 0;

    module_mutex_lock();

    list_node_t *curr = module_ctx.matched_mods.next;
    while (curr != &module_ctx.matched_mods) {
        /* Captured up front: see this function's THREADING note. */
        list_node_t *next = curr->next;
        module_entry_t *e = container_of(curr, module_entry_t, matches);
        module_t *m = &e->mod;

        if (m->opt_nr == 0 || e->args_nr == 0) {
            /* Terminal matched module with action (e.g. container module) */
            if (next == &module_ctx.matched_mods && m->action) {
                int (*action_fn)(int, module_option_t **) = m->action;

                module_mutex_unlock();
                int action_ret = action_fn(0, NULL);
                module_mutex_lock();

                if (action_ret) {
                    pr_info("module action failed status on execution");
                    ret = -1;
                    goto out;
                }
            }
            curr = next;
            continue;
        }

        /* '+' + ':' + up to 3 chars per option + '\0' */
        char optstring[m->opt_nr * 3 + 3];
        struct option long_options[m->opt_nr + 1];
        size_t optstr_idx = 0;

        /*
         * Leading '+' makes getopt_long() stop at the first non-option
         * argument, instead of permuting them to the end of argv[].
         */
        optstring[optstr_idx++] = '+';

        /*
         * Leading ':' makes getopt_long() distinguish an unknown option
         * ('?') from a missing argument (':').
         */
        optstring[optstr_idx++] = ':';

        for (size_t i = 0; i < m->opt_nr; i++) {
            module_option_t *o = m->options[i];

            long_options[i].name = o->l_opt;
            long_options[i].has_arg = o->has_arg;
            long_options[i].flag = NULL;
            /* Long-only options get a synthetic value above the char range. */
            long_options[i].val =
                o->s_opt ? (int)(unsigned char)o->s_opt : (int)(1000 + i);

            if (o->s_opt != 0) {
                optstring[optstr_idx++] = o->s_opt;
                if (o->has_arg == required_argument) {
                    optstring[optstr_idx++] = ':';
                } else if (o->has_arg == optional_argument) {
                    optstring[optstr_idx++] = ':';
                    optstring[optstr_idx++] = ':';
                }
            }
        }

        /* Terminators required by getopt_long(). */
        long_options[m->opt_nr] = (struct option){0};
        optstring[optstr_idx] = '\0';

        pr_debug(
            "Generated optstring='%s' for %zu options in module '%s'",
            optstring,
            (size_t)m->opt_nr,
            m->name);

        int parse_res = parse_cli_modules_options(
            (int)e->args_nr, e->args, optstring, long_options, m);
        if (parse_res < 0) {
            ret = -1;
            goto out;
        }
        if (parse_res > 0) {
            /* Help requested and displayed, exit gracefully without running
             * action */
            ret = 0;
            goto out;
        }

        /* Hand the handler only the options that were set on the CLI. */
        module_option_t **matched_opts =
            malloc(m->opt_nr * sizeof(module_option_t *));
        if (!matched_opts) {
            pr_error("Failed to allocate matched options array");
            ret = -1;
            goto out;
        }

        int matched_nr = 0;
        for (unsigned int i = 0; i < m->opt_nr; i++) {
            if (m->options[i]->is_set)
                matched_opts[matched_nr++] = m->options[i];
        }

        int (*action_fn)(int, module_option_t **) = m->action;

        module_mutex_unlock();
        int action_ret = action_fn(matched_nr, matched_opts);
        module_mutex_lock();

        free(matched_opts);

        if (action_ret) {
            pr_info("module action failed status on execution");

#ifdef CLI_IGNORE_MODULE_ERRORS
            curr = next;
            continue;
#endif
            ret = -1;
            goto out;
        }

        curr = next;
    }

out:
    module_mutex_unlock();
    return ret;
}

/**
 * @brief Runs the whole CLI pipeline: match, dispatch, then clean up.
 *
 * Cleanup runs exactly once, on both the success and the error path, so
 * nothing leaks if a stage fails.
 *
 * @param[in] argv NULL-terminated argument vector.
 *
 * @return 0 on success, -1 on error.
 */
static int parse_cli(char *const *argv)
{
    int ret = -1;

    if (parse_cli_modules(argv) != 0)
        goto out;

    if (cli_parsing() != 0)
        goto out;

    ret = 0;

out:
    cleanup_cli_modules();

    return ret;
}

int launch_cli(char *const *argv)
{
    bool not_running = false;
    if (!atomic_compare_exchange_strong(
            &module_ctx.cli_running, &not_running, true)) {
        pr_error("launch_cli() is already running on another invocation");
        return -1;
    }

    int ret = parse_cli(argv);

    atomic_store(&module_ctx.cli_running, false);

    return ret;
}

/**
 * @brief Renders CLI usage instructions for a module.
 *
 * @param[in] stream Output stream (defaults to stderr if NULL).
 * @param[in] m      Module metadata.
 *
 * @return 0 on success, -1 if the module pointer is invalid.
 */
static void module_usage_impl(FILE *stream, const module_t *m)
{
    const char *progr_name = module_get_prog_name();
    const char *name = strrchr(progr_name, '/');

    name = name ? name + 1 : progr_name;

    pr_module_short_usage(stream, m);

    if (m->options && m->opt_nr > 0) {
        fprintf(stream, "Options:\n\n");

        for (size_t i = 0; i < m->opt_nr; i++) {
            const module_option_t *o = m->options[i];
            char opt_buf[64];

            if (o->s_opt && o->l_opt) {
                if (o->has_arg == required_argument) {
                    snprintf(
                        opt_buf,
                        sizeof(opt_buf),
                        "  -%c, --%s %s",
                        o->s_opt,
                        o->l_opt,
                        o->arg_name ? o->arg_name : "<val>");
                } else {
                    snprintf(
                        opt_buf,
                        sizeof(opt_buf),
                        "  -%c, --%s",
                        o->s_opt,
                        o->l_opt);
                }
            } else if (o->l_opt) {
                if (o->has_arg == required_argument) {
                    snprintf(
                        opt_buf,
                        sizeof(opt_buf),
                        "      --%s %s",
                        o->l_opt,
                        o->arg_name ? o->arg_name : "<val>");
                } else {
                    snprintf(opt_buf, sizeof(opt_buf), "      --%s", o->l_opt);
                }
            } else if (o->s_opt) {
                if (o->has_arg == required_argument) {
                    snprintf(
                        opt_buf,
                        sizeof(opt_buf),
                        "  -%c %s",
                        o->s_opt,
                        o->arg_name ? o->arg_name : "<val>");
                } else {
                    snprintf(opt_buf, sizeof(opt_buf), "  -%c", o->s_opt);
                }
            } else {
                continue;
            }

            fprintf(stream, "%-32s %s\n", opt_buf, o->desc ? o->desc : "");
        }
        fprintf(stream, "\n");
    }

    /*
     * Lists this module's sub-modules, if any: reads module_ctx.root_entry/
     * root_sub_modules/entry->sub_modules, so it needs the lock - safe to
     * take here even when already held by a caller higher up the stack
     * (cli_parsing() -> parse_cli_modules_options() -> pr_module_usage()),
     * since module_mutex_lock() is recursive.
     */
    module_mutex_lock();

    module_entry_t *entry = to_module_entry((module_t *)m);
    list_node_t *sub_mods = (entry == module_ctx.root_entry)
                                ? &module_ctx.root_sub_modules
                                : (entry ? &entry->sub_modules : NULL);

    if (sub_mods && !list_empty(sub_mods)) {
        fprintf(stream, "Available commands:\n");
        list_node_t *curr;
        list_for_each(curr, sub_mods)
        {
            module_entry_t *child =
                container_of(curr, module_entry_t, siblings);
            char cmd_buf[32];
            snprintf(
                cmd_buf,
                sizeof(cmd_buf),
                "  %s",
                child->mod.name ? child->mod.name : "");
            fprintf(
                stream,
                "%-16s %s\n",
                cmd_buf,
                child->mod.usage ? child->mod.usage : "");
        }
        fprintf(stream, "\n");
    }

    module_mutex_unlock();
}

int pr_module_usage(FILE *stream, const module_t *m)
{
    if (!m)
        return -1;

    if (!stream)
        stream = stderr;

    module_usage_impl(stream, m);
    return 0;
}

void pr_module_short_usage(FILE *stream, const module_t *m)
{
    if (!m)
        return;

    if (!stream)
        stream = stderr;

    const char *progr_name = module_get_prog_name();
    const char *name = strrchr(progr_name, '/');
    name = name ? name + 1 : progr_name;

    module_mutex_lock();

    const module_entry_t *entry = to_module_entry((module_t *)m);

    size_t depth = 0;
    const module_entry_t *curr = entry;
    while (curr) {
        depth++;
        curr = curr->parent;
    }

    const module_t *chain[depth];
    size_t idx = depth;
    curr = entry;
    while (curr) {
        chain[--idx] = &(curr->mod);
        curr = curr->parent;
    }

#define PRINT_USAGE(fmt, ...)                                                  \
    do {                                                                       \
        if (stream == stderr)                                                  \
            pr_red(stream, fmt, ##__VA_ARGS__);                                \
        else                                                                   \
            pr_yellow(stream, fmt, ##__VA_ARGS__);                             \
    } while (0)

    PRINT_USAGE("Usage: %s", name);

    /* avoid to print the root module name */
    if (m != (const module_t *)module_ctx.root_entry) {
        for (size_t i = 0; i < depth; i++) {
            PRINT_USAGE(" %s", chain[i]->name ? chain[i]->name : "module");
        }
    }

    PRINT_USAGE(" %s\n", m->usage ? m->usage : "[OPTIONS]");

#undef PRINT_USAGE

    module_mutex_unlock();
}

/**
 * @brief Writes one `case` arm: a module path mapped to its candidates.
 *
 * Candidates are @p mod's long options (as `--name`) followed by @p
 * children's names. Short options are skipped - long options are what a
 * completion list should show.
 */
static void write_completion_arm(
    FILE *out, const char *path, const module_t *mod, list_node_t *children)
{
    fprintf(out, "\"%s\")\n    candidates=\"", path);

    bool first = true;

    if (mod) {
        for (unsigned int i = 0; i < mod->opt_nr; i++) {
            const char *l_opt = mod->options[i]->l_opt;
            if (!l_opt)
                continue;
            fprintf(out, "%s--%s", first ? "" : " ", l_opt);
            first = false;
        }
    }

    list_node_t *curr;
    list_for_each(curr, children)
    {
        module_entry_t *child = container_of(curr, module_entry_t, siblings);
        fprintf(out, "%s%s", first ? "" : " ", child->mod.name);
        first = false;
    }

    fprintf(out, "\"\n    ;;\n");
}

/**
 * @brief Emits one arm per module under @p modules, depth-first.
 *
 * @p parent_path is the space-joined names of modules above this level, or
 * "" for the top level. Paths are capped at 255 chars (snprintf-truncated);
 * fine for any reasonable module tree.
 */
static void
write_completion_tree(FILE *out, const char *parent_path, list_node_t *modules)
{
    list_node_t *curr;
    list_for_each(curr, modules)
    {
        module_entry_t *e = container_of(curr, module_entry_t, siblings);

        char path[256];
        if (parent_path[0] == '\0')
            snprintf(path, sizeof(path), "%s", e->mod.name);
        else
            snprintf(path, sizeof(path), "%s %s", parent_path, e->mod.name);

        write_completion_arm(out, path, &e->mod, &e->sub_modules);
        write_completion_tree(out, path, &e->sub_modules);
    }
}

/**
 * @brief Writes everything before the `case "$path" in` line of the
 * POSIX sh completion script: shebang, header comment, and the lookup
 * function's word-parsing prologue.
 */
static void
write_sh_header(FILE *out, const char *base, const char *output_path)
{
    fprintf(out, "#!/bin/sh\n");
    fprintf(out, "# Auto-generated by %s - do not edit by hand.\n", base);
    fprintf(out, "#\n");
    fprintf(
        out, "# _%s_complete() is strict POSIX sh: given every word\n", base);
    fprintf(out, "# typed after the program name,  the last one being the\n");
    fprintf(out, "# (possibly partial) word under the cursor, it prints the\n");
    fprintf(out, "# matching candidates, one per line. Try it directly:\n");
    fprintf(out, "#   . %s && _%s_complete net \"\"\n", output_path, base);
    fprintf(out, "\n_%s_complete() {\n", base);
    fprintf(out, "    cur=\"\"\n");
    fprintf(out, "    path=\"\"\n");
    fprintf(out, "    last_idx=$#\n");
    fprintf(out, "    i=0\n");
    fprintf(out, "    for w in \"$@\"; do\n");
    fprintf(out, "        i=$((i + 1))\n");
    fprintf(out, "        if [ \"$i\" -eq \"$last_idx\" ]; then\n");
    fprintf(out, "            cur=$w\n");
    fprintf(out, "        elif [ -z \"$path\" ]; then\n");
    fprintf(out, "            path=$w\n");
    fprintf(out, "        else\n");
    fprintf(out, "            path=\"$path $w\"\n");
    fprintf(out, "        fi\n");
    fprintf(out, "    done\n");
    fprintf(out, "\n    candidates=\"\"\n");
    fprintf(out, "    case \"$path\" in\n");
}

/**
 * @brief Writes everything after the `esac` of the POSIX sh completion
 * script: the prefix-filtering loop, and a bash `complete -F` wrapper for
 * convenience.
 */
static void write_sh_footer(FILE *out, const char *base)
{
    fprintf(out, "    esac\n\n");
    fprintf(out, "    for c in $candidates; do\n");
    fprintf(out, "        case \"$c\" in\n");
    fprintf(out, "            \"$cur\"*) printf '%%s\\n' \"$c\" ;;\n");
    fprintf(out, "        esac\n");
    fprintf(out, "    done\n");
    fprintf(out, "}\n\n");

    fprintf(out, "# Optional bash wiring , hidden in eval so a POSIX-only\n");
    fprintf(out, "# shell never has to parse bash-only array syntax.\n");
    fprintf(out, "if [ -n \"${BASH_VERSION:-}\" ]; then\n");
    fprintf(out, "eval '\n");
    fprintf(out, "_%s_bash_complete() {\n", base);
    fprintf(out, "    COMPREPLY=()\n");
    fprintf(out, "    while IFS= read -r line; do\n");
    fprintf(out, "        COMPREPLY+=(\"$line\")\n");
    fprintf(out, "    done <<EOF\n");
    fprintf(out, "$(_%s_complete \"${COMP_WORDS[@]:1:COMP_CWORD}\")\n", base);
    fprintf(out, "EOF\n");
    fprintf(out, "}\n");
    fprintf(out, "complete -F _%s_bash_complete %s\n", base, base);
    fprintf(out, "'\n");
    fprintf(out, "fi\n");
}

/**
 * @brief Writes everything before the `case "$path" in` line of the bash
 * completion script: shebang, header comment, and the completion
 * function's prologue (reads COMP_WORDS/COMP_CWORD directly).
 */
static void
write_bash_header(FILE *out, const char *base, const char *output_path)
{
    fprintf(out, "#!/usr/bin/env bash\n");
    fprintf(out, "# Auto-generated by %s - do not edit by hand.\n", base);
    fprintf(out, "# Bash completion for %s.\n", base);
    fprintf(out, "#   . %s\n", output_path);
    fprintf(out, "\n_%s_complete() {\n", base);
    fprintf(out, "    local cur path i\n");
    fprintf(out, "    cur=${COMP_WORDS[COMP_CWORD]}\n");
    fprintf(out, "    path=\"\"\n");
    fprintf(out, "    for ((i = 1; i < COMP_CWORD; i++)); do\n");
    fprintf(out, "        if [ -z \"$path\" ]; then\n");
    fprintf(out, "            path=${COMP_WORDS[i]}\n");
    fprintf(out, "        else\n");
    fprintf(out, "            path=\"$path ${COMP_WORDS[i]}\"\n");
    fprintf(out, "        fi\n");
    fprintf(out, "    done\n");
    fprintf(out, "\n    local candidates=\"\"\n");
    fprintf(out, "    case \"$path\" in\n");
}

/**
 * @brief Writes everything after the `esac` of the bash completion script:
 * COMPREPLY filled via `compgen` (handles prefix matching and quoting the
 * way bash's own completion expects), and the `complete -F` registration.
 */
static void write_bash_footer(FILE *out, const char *base)
{
    fprintf(out, "    esac\n\n");
    fprintf(out, "    COMPREPLY=($(compgen -W \"$candidates\" -- \"$cur\"))\n");
    fprintf(out, "}\n\n");
    fprintf(out, "complete -F _%s_complete %s\n", base, base);
}

/**
 * @brief Shared core of generate_sh_completion()/generate_bash_completion():
 * opens @p output_path, writes @p write_header, one `case` arm per
 * registered module (root included), @p write_footer, then closes the file.
 *
 * @note Thread-safe: holds module_ctx.mutex for the whole walk, so the tree
 * can't change shape mid-generation.
 */
static int write_completion_script(
    const char *output_path,
    void (*write_header)(FILE *out, const char *base, const char *output_path),
    void (*write_footer)(FILE *out, const char *base))
{
    if (!output_path)
        return -1;

    const char *prog_name = module_get_prog_name();
    if (!prog_name)
        return -1;

    FILE *out = fopen(output_path, "w");
    if (!out) {
        pr_error("Failed to open '%s' for writing", output_path);
        return -1;
    }

    const char *base = strrchr(prog_name, '/');
    base = base ? base + 1 : prog_name;

    module_mutex_lock();

    write_header(out, base, output_path);

    write_completion_arm(
        out,
        "",
        module_ctx.root_entry ? &module_ctx.root_entry->mod : NULL,
        &module_ctx.root_sub_modules);
    write_completion_tree(out, "", &module_ctx.root_sub_modules);

    write_footer(out, base);

    module_mutex_unlock();

    fclose(out);
    pr_info("Generated completion script at '%s'", output_path);
    return 0;
}

/**
 * @brief Generates a POSIX sh completion script for the whole registry.
 *
 * Uses the current registered program name from module_ctx.prog_name.
 */
int generate_sh_completion(const char *output_path)
{
    return write_completion_script(
        output_path, write_sh_header, write_sh_footer);
}

/**
 * @brief Generates a bash completion script for the whole registry.
 *
 * Uses the current registered program name from module_ctx.prog_name.
 */
int generate_bash_completion(const char *output_path)
{
    return write_completion_script(
        output_path, write_bash_header, write_bash_footer);
}