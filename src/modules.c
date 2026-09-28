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
 * @details   Modules are registered in a tree (root module, top-level modules,
 *            nested sub-modules). A command line is processed in three stages:
 *            1. parse_cli_modules(): walks argv, matches module names against
 *               the tree and captures raw option tokens on the active module.
 *            2. cli_parsing(): for every matched module, runs getopt_long()
 *               over its captured tokens, then invokes its action handler
 *               with the options that were actually set.
 *            3. cleanup_cli_modules(): releases the whole registry.
 *
 */

#define prfx_fmt "modules: "

#include <ctype.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "modules.h"

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
     * with add_module(). Also the scope searched first by the parser.
     */
    list_node_t root_sub_modules;

    /**
     * Head of the list of modules activated during argument parsing, in
     * command-line order. Linked through module_entry_t::matches.
     */
    list_node_t matched_mods;
};

static struct module_ctx_struct module_ctx = {
    .root_entry = NULL,
    .root_sub_modules = {&module_ctx.root_sub_modules,
                         &module_ctx.root_sub_modules},
    .matched_mods = {&module_ctx.matched_mods, &module_ctx.matched_mods},
};

static void free_module_tree(list_node_t *head);

/**
 * @brief Releases every heap allocation owned by a public module_t.
 *
 * Frees the name, the usage string and the options array, including each
 * option's duplicated strings. NULL-safe at every level, so it can also
 * clean up a partially built module. The structure is zeroed afterwards,
 * leaving no dangling pointer behind.
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
    module_option_t **options,
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
            if (dup_option(&mod->options[i], options[i]) != 0)
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
    module_option_t **options,
    unsigned int opt_nr,
    int (*action)(int optc, module_option_t **optv),
    unsigned int level)
{
    module_entry_t *entry = calloc(1, sizeof(*entry));
    if (!entry)
        return NULL;

    if (init_module_public(&entry->mod, name, usage, options, opt_nr, action)
        != 0) {
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
 * @param[in] head Head of the list to empty. Left as a valid empty list, so
 * it can safely be reused or emptied again.
 */
static void free_module_tree(list_node_t *head)
{
    if (!head)
        return;

    list_node_t *curr = head->next;
    while (curr != head) {
        list_node_t *next = curr->next;

        destroy_module_entry(container_of(curr, module_entry_t, siblings));
        curr = next;
    }
}

/**
 * @brief Destroys the global root module, if any.
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
 */
module_t *set_root_module(
    const char *name,
    const char *usage,
    module_option_t **options,
    unsigned int opt_nr,
    int (*action)(int optc, module_option_t **optv))
{
    if (!name || !action)
        return NULL;

    module_entry_t *root = module_ctx.root_entry;

    if (!root) {
        root = create_module_entry(name, usage, options, opt_nr, action, 0);
        if (!root)
            return NULL;

        module_ctx.root_entry = root;
        return &root->mod;
    }

    module_t fresh;
    if (init_module_public(&fresh, name, usage, options, opt_nr, action) != 0)
        return NULL;

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
 */
module_t *add_module(
    const char *name,
    const char *usage,
    module_option_t **options,
    unsigned int opt_nr,
    int (*action)(int optc, module_option_t **optv))
{
    if (!name || !action)
        return NULL;

    module_entry_t *entry =
        create_module_entry(name, usage, options, opt_nr, action, 0);
    if (!entry)
        return NULL;

    list_add_tail(&module_ctx.root_sub_modules, &entry->siblings);

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
 */
module_t *add_submodule(
    module_t *parent_mod,
    const char *name,
    const char *usage,
    module_option_t **options,
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
    list_add_tail(&parent_entry->sub_modules, &child->siblings);

    return &child->mod;
}

/**
 * @brief Looks up a module by name in one sub-modules list.
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
 */
static void cleanup_cli_modules(void)
{
    free_module_tree(&module_ctx.root_sub_modules);
    free_root_entry();
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
 * @param[in] argv NULL-terminated argument vector; argv[0] is skipped.
 *
 * @return 0 on success, -1 on error.
 */
static int parse_cli_modules(char *const *argv)
{
    if (!argv || !*argv)
        return 0;

    list_node_t *current_sub_modules = &module_ctx.root_sub_modules;
    module_entry_t *current_mod = module_ctx.root_entry;

    for (char *const *args = argv + 1; *args != NULL; args++) {
        const char *arg = *args;

        /* Option token: attach it to the active module. */
        if (arg[0] == '-') {
            if (!current_mod) {
                pr_error(
                    "Option '%s' is not associated with any active module",
                    arg);
                return -1;
            }

            if (current_mod == module_ctx.root_entry
                && !current_mod->matched) {
                current_mod->matched = true;
                list_add_tail(&module_ctx.matched_mods, &current_mod->matches);
            }

            char **new_args = realloc(
                current_mod->args, (current_mod->args_nr + 1) * sizeof(char *));
            if (!new_args) {
                pr_error("Failed to create an argument");
                return -1;
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
        if (!found) {
            pr_error("Module or submodule '%s' unknown in this context", arg);
            return -1;
        }

        if (found->matched) {
            pr_error("Module '%s' has already been specified", arg);
            return -1;
        }

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
            "Module: <%s> [level='%u', '%u' option(s)]",
            e->mod.name,
            e->level,
            e->args_nr);
        for (unsigned int i = 0; i < e->args_nr; i++)
            pr_debug("%s", e->args[i]);
    }
#endif

    return 0;
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
 * @return 0 on success, -1 on error.
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
                parse_argc, parse_argv, optstring, long_options, &long_idx))
           != -1) {
        pr_debug("parsed opt=%d ('%c'), optind=%d", opt, printable(opt), optind);

        if (opt == '?') {
            pr_red(stderr, "Error: Unrecognized option: '-%c'\n",
                   printable(optopt));
            if (m->usage)
                fprintf(stderr, "Usage: %s\n", m->usage);
            ret = -1;
            goto out;
        }

        if (opt == ':') {
            pr_red(stderr, "Error: Option requires an argument: '-%c'\n",
                   printable(optopt));
            if (m->usage)
                fprintf(stderr, "Usage: %s\n", m->usage);
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
                if (m->options[i]->s_opt
                    && (int)(unsigned char)m->options[i]->s_opt == opt) {
                    matched = m->options[i];
                    break;
                }
            }
        }

        if (!matched) {
            pr_error("unhandled option code: %d", opt);
            ret = -1;
            goto out;
        }

        matched->is_set = true;
        matched->arg_val = optarg;
        pr_debug("matched option '--%s' (-%c), val='%s'",
                 matched->l_opt ? matched->l_opt : "",
                 matched->s_opt ? matched->s_opt : ' ',
                 matched->arg_val ? matched->arg_val : "none");

        /* getopt_long() only updates long_idx for long options. */
        long_idx = -1;
    }

    pr_debug("Parsing completed successfully for module: %s",
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
 * @return 0 on success, -1 on error.
 */
static int cli_parsing(void)
{
    list_node_t *curr_node;

    list_for_each(curr_node, &module_ctx.matched_mods)
    {
        module_entry_t *e = container_of(curr_node, module_entry_t, matches);
        module_t *m = &e->mod;

        if (m->opt_nr == 0 || e->args_nr == 0)
            continue;

        char optstring[m->opt_nr * 3 + 2];
        struct option long_options[m->opt_nr + 1];
        size_t optstr_idx = 0;

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

        pr_debug("Generated optstring='%s' for %zu options in module '%s'",
                 optstring, (size_t)m->opt_nr, m->name);

        if (parse_cli_modules_options(
                (int)e->args_nr, e->args, optstring, long_options, m)
            != 0)
            return -1;

        /* Hand the handler only the options that were set on the CLI. */
        module_option_t **matched_opts =
            malloc(m->opt_nr * sizeof(module_option_t *));
        if (!matched_opts) {
            pr_error("Failed to allocate matched options array");
            return -1;
        }

        int matched_nr = 0;
        for (unsigned int i = 0; i < m->opt_nr; i++) {
            if (m->options[i]->is_set)
                matched_opts[matched_nr++] = m->options[i];
        }

        int action_ret = m->action(matched_nr, matched_opts);
        free(matched_opts);

        if (action_ret) {
            pr_info("module action failed status on execution");

#ifdef CLI_IGNORE_MODULE_ERRORS
            continue;
#endif
            return -1;
        }
    }

    return 0;
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

    if (ret != 0)
        pr_error("Failed to parse command line options");

    return ret;
}

int launch_cli(char *const *argv)
{
    return parse_cli(argv);
}