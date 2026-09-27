// SPDX-License-Identifier: GPL-2.0
//
// vim: set ts=8 sw=8 noet tw=80 cc=80 fo+=t :

/**
 * @file      modules.c
 * @author    midnight walker
 * @brief     Execute options and arguments with handlers.
 * @version   0.3
 * @date      2026-09-25
 * @copyright GNU General Public License v2.0
 *
 */

#define prfx_fmt "modules: "

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "modules.h"

/**
 * @brief Pointer to the global root module entry.
 *
 * Holds top-level application options passed before any explicit sub-command.
 * Initialized to NULL and allocated via set_root_module().
 */
static module_entry_t *root_entry = NULL;

/**
 * @brief Internal head of the root-level sub-modules intrusive list.
 *
 * Self-initialized circular doubly linked list head containing all top-level
 * modules (Level 0).
 */
static list_node_t root_sub_modules = {&root_sub_modules, &root_sub_modules};

/**
 * @brief Internal head of the matched modules intrusive list.
 *
 * Tracks modules activated sequentially during command-line argument parsing.
 */
static list_node_t matched_mods = {&matched_mods, &matched_mods};

static char *prog_name;

static void free_module_tree(list_node_t *head);

/**
 * @brief Allocates and initializes a new internal module entry.
 *
 * Performs a deep copy of the module name, usage string, and its associated
 * options array. Initializes all internal list nodes and hierarchy fields.
 *
 * @param[in] name    Name of the module.
 * @param[in] usage   Usage syntax pattern (e.g. "[options] <target>"), or NULL
 * if none.
 * @param[in] options Pointer to an array of option pointers, or NULL if none.
 * @param[in] opt_nr  Total number of options in the array.
 * @param[in] action  Module execution handler.
 * @param[in] level   Depth level of the module within the hierarchy tree.
 *
 * @return Pointer to the allocated module_entry_t structure, or NULL on
 * failure.
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

    /* Deep copy of the module name */
    entry->mod.name = strdup(name);
    if (!entry->mod.name) {
        free(entry);
        return NULL;
    }

    /* Deep copy of the usage syntax string (if provided) */
    if (usage) {
        entry->mod.usage = strdup(usage);
        if (!entry->mod.usage) {
            free(entry->mod.name);
            free(entry);
            return NULL;
        }
    }

    /* Deep copy of the options array */
    if (options && opt_nr > 0) {
        entry->mod.options = malloc(opt_nr * sizeof(module_option_t *));
        if (!entry->mod.options) {
            free((char *)entry->mod.usage);
            free(entry->mod.name);
            free(entry);
            return NULL;
        }

        for (unsigned int i = 0; i < opt_nr; i++) {
            entry->mod.options[i] = malloc(sizeof(module_option_t));
            if (!entry->mod.options[i]) {
                /* Cleanup previously allocated options on error */
                for (unsigned int j = 0; j < i; j++) {
                    free((char *)entry->mod.options[j]->l_opt);
                    free((char *)entry->mod.options[j]->arg_name);
                    free((char *)entry->mod.options[j]->desc);
                    free(entry->mod.options[j]);
                }
                free(entry->mod.options);
                free((char *)entry->mod.usage);
                free(entry->mod.name);
                free(entry);
                return NULL;
            }
            memcpy(entry->mod.options[i], options[i], sizeof(module_option_t));

            /* Deep copy of internal string fields */
            if (options[i]->l_opt)
                entry->mod.options[i]->l_opt = strdup(options[i]->l_opt);
            if (options[i]->arg_name)
                entry->mod.options[i]->arg_name = strdup(options[i]->arg_name);
            if (options[i]->desc)
                entry->mod.options[i]->desc = strdup(options[i]->desc);
        }
        entry->mod.opt_nr = opt_nr;
    }

    entry->level = level;
    entry->mod.action = action;
    list_init(&entry->sub_modules);
    list_init(&entry->siblings);
    list_init(&entry->matches);

    return entry;
}

/**
 * @brief Configures the root entry used as a fallback context when CLI options
 * are provided before any sub-command. If already initialized, existing
 * allocations are cleaned up.
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

    /* Allocate root_entry on first call */
    if (!root_entry) {
        root_entry =
            create_module_entry(name, usage, options, opt_nr, action, 0);
        if (!root_entry)
            return NULL;
        return &root_entry->mod;
    }

    /* Clean up previous resources if re-initialized */
    free(root_entry->mod.name);
    free((char *)root_entry->mod.usage);

    if (root_entry->mod.options) {
        for (unsigned int i = 0; i < root_entry->mod.opt_nr; i++) {
            if (root_entry->mod.options[i]) {
                free((char *)root_entry->mod.options[i]->l_opt);
                free((char *)root_entry->mod.options[i]->arg_name);
                free((char *)root_entry->mod.options[i]->desc);
                free(root_entry->mod.options[i]);
            }
        }
        free(root_entry->mod.options);
    }

    root_entry->mod.name = NULL;
    root_entry->mod.usage = NULL;
    root_entry->mod.options = NULL;
    root_entry->mod.opt_nr = 0;

    if (root_entry->args) {
        free(root_entry->args);
        root_entry->args = NULL;
        root_entry->args_nr = 0;
    }

    /* Deep copy of name */
    root_entry->mod.name = strdup(name);
    if (!root_entry->mod.name)
        return NULL;

    /* Deep copy of usage syntax string */
    if (usage) {
        root_entry->mod.usage = strdup(usage);
        if (!root_entry->mod.usage) {
            free(root_entry->mod.name);
            root_entry->mod.name = NULL;
            return NULL;
        }
    } else {
        root_entry->mod.usage = NULL;
    }

    /* Deep copy of options array */
    if (options && opt_nr > 0) {
        root_entry->mod.options = malloc(opt_nr * sizeof(module_option_t *));
        if (!root_entry->mod.options) {
            free((char *)root_entry->mod.usage);
            free(root_entry->mod.name);
            root_entry->mod.name = NULL;
            root_entry->mod.usage = NULL;
            return NULL;
        }

        for (unsigned int i = 0; i < opt_nr; i++) {
            root_entry->mod.options[i] = malloc(sizeof(module_option_t));
            if (!root_entry->mod.options[i]) {
                for (unsigned int j = 0; j < i; j++) {
                    free((char *)root_entry->mod.options[j]->l_opt);
                    free((char *)root_entry->mod.options[j]->arg_name);
                    free((char *)root_entry->mod.options[j]->desc);
                    free(root_entry->mod.options[j]);
                }
                free(root_entry->mod.options);
                free((char *)root_entry->mod.usage);
                free(root_entry->mod.name);
                root_entry->mod.name = NULL;
                root_entry->mod.usage = NULL;
                return NULL;
            }
            memcpy(
                root_entry->mod.options[i],
                options[i],
                sizeof(module_option_t));

            if (options[i]->l_opt)
                root_entry->mod.options[i]->l_opt = strdup(options[i]->l_opt);
            if (options[i]->arg_name)
                root_entry->mod.options[i]->arg_name =
                    strdup(options[i]->arg_name);
            if (options[i]->desc)
                root_entry->mod.options[i]->desc = strdup(options[i]->desc);
        }
        root_entry->mod.opt_nr = opt_nr;
    }

    else {
        root_entry->mod.options = NULL;
        root_entry->mod.opt_nr = 0;
    }

    free_module_tree(&root_entry->sub_modules);

    root_entry->level = 0;
    root_entry->mod.action = action;
    list_init(&root_entry->sub_modules);
    list_init(&root_entry->siblings);
    list_init(&root_entry->matches);

    return &root_entry->mod;
}

/**
 * @brief Frees the standalone global root module and all its allocated memory.
 */
static void free_root_entry(void)
{
    if (!root_entry)
        return;

    list_del(&root_entry->matches);

    free_module_tree(&root_entry->sub_modules);

    free(root_entry->mod.name);
    free((char *)root_entry->mod.usage);

    if (root_entry->mod.options) {
        for (unsigned int i = 0; i < root_entry->mod.opt_nr; i++) {
            if (root_entry->mod.options[i]) {
                free((char *)root_entry->mod.options[i]->l_opt);
                free((char *)root_entry->mod.options[i]->arg_name);
                free((char *)root_entry->mod.options[i]->desc);
                free(root_entry->mod.options[i]);
            }
        }
        free(root_entry->mod.options);
    }

    if (root_entry->args)
        free(root_entry->args);

    free(root_entry);
    root_entry = NULL;
}

/**
 * @brief Allocates and initializes a root-level module entry, then appends it
 * to the global root sub-modules list.
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

    /* Create the module entry at root level (0) */
    module_entry_t *entry =
        create_module_entry(name, usage, options, opt_nr, action, 0);
    if (!entry)
        return NULL;

    /* Append to the global root sub-modules list */
    list_add_tail(&root_sub_modules, &entry->siblings);

    return &entry->mod;
}

/**
 * @brief Allocates and initializes a new child module entry with a hierarchy
 * level incremented relative to its parent, sets the parent link, and appends
 * it to the parent's sub-modules list.
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

    /* Retrieve the private entry container from the public module pointer */
    module_entry_t *parent_entry = to_module_entry(parent_mod);

    /* Create the child entry with its depth level incremented */
    module_entry_t *child = create_module_entry(
        name, usage, options, opt_nr, action, parent_entry->level + 1);
    if (!child)
        return NULL;

    /* Link child to parent and append to the parent's sub-modules list */
    child->parent = parent_entry;
    list_add_tail(&parent_entry->sub_modules, &child->siblings);

    return &child->mod;
}

/**
 * @brief Searches for a module by its name in a specific sub-modules list.
 *
 * Iterates through the intrusive list starting at the provided head and
 * compares each module entry's name with the target name.
 *
 * @param[in] sub_modules_head Pointer to the head node of the sub-modules list.
 * @param[in] name             Name of the module to find.
 *
 * @return Pointer to the matching module_entry_t structure if found, or NULL
 * otherwise.
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
        if (entry->mod.name && strcmp(entry->mod.name, name) == 0) {
            return entry;
        }
    }
    return NULL;
}

/**
 * @brief Recursively frees a module tree and all its allocated resources.
 *
 * Traverses the sub-module list, recursively frees child trees, releases all
 * heap-allocated fields (name, options, argument arrays), and frees the module
 * entries.
 *
 * @param[in] head Pointer to the head node of the module tree or sub-tree to
 * free. Left as a valid, empty (self-referencing) list head on return, so it
 * remains safe to reuse (e.g. root_sub_modules across further add_module()/
 * launch_cli() cycles) or to free again.
 */
static void free_module_tree(list_node_t *head)
{
    if (!head)
        return;

    list_node_t *curr = head->next;
    while (curr != head) {
        list_node_t *next = curr->next;
        module_entry_t *entry = container_of(curr, module_entry_t, siblings);

        /* Recursively free child sub-modules */
        free_module_tree(&entry->sub_modules);

        list_del(&entry->matches);
        list_del(&entry->siblings);

        /* Free allocated fields in public module_t structure */
        free(entry->mod.name);
        free((char *)entry->mod.usage);
        if (entry->mod.options) {
            for (unsigned int i = 0; i < entry->mod.opt_nr; i++) {
                if (entry->mod.options[i]) {
                    free((char *)entry->mod.options[i]->l_opt);
                    free((char *)entry->mod.options[i]->arg_name);
                    free((char *)entry->mod.options[i]->desc);
                    free(entry->mod.options[i]);
                }
            }
            free(entry->mod.options);
        }

        /* Free captured CLI argument array */
        if (entry->args)
            free(entry->args);

        free(entry);
        curr = next;
    }
}

/**
 * @brief Releases every module resource created for this CLI invocation.
 */
static void cleanup_cli_modules(void)
{
    free_module_tree(&root_sub_modules);
    free_root_entry();
}

/**
 * @brief Parses command-line arguments, activates matching modules, and
 * captures options.
 * Traverses `argv` sequentially until NULL to match positional arguments
 * against registered module hierarchies and captures option flags into active
 * module structures.
 *
 * @note Does not free anything itself: matched modules and their captured
 * arguments must remain alive for cli_parsing() to consume afterward.
 * Callers are responsible for calling cleanup_cli_modules() once, after
 * cli_parsing() has run (see parse_cli()).
 *
 * @param[in] argv Array of command-line argument strings (NULL-terminated).
 *
 * @return 0 on success, -1 on error
 */
static int parse_cli_modules(char *const *argv)
{
    if (!argv || !*argv)
        return 0;

    int ret = -1;
    list_node_t *current_sub_modules = &root_sub_modules;

    /* Default context points to root_entry if configured, enabling global
     * options capture */
    module_entry_t *current_mod = root_entry;

    for (char **args = (char **)argv + 1; *args != NULL; args++) {
        const char *arg = *args;

        /* Option processing (arguments starting with '-') */
        if (arg[0] == '-') {
            if (!current_mod) {
                pr_error(
                    "Option '%s' is not associated with any active module "
                    "(root_entry=(null))",
                    arg);
                print_red(
                    stderr,
                    "Internal error while parsing option(s) '%s' failed",
                    arg);
                goto out;
            }

            /*
             * Lazily record the root module as "matched" the first
             * time it captures a global option.
             */
            if (current_mod == root_entry && !root_entry->matched) {
                root_entry->matched = true;
                list_add_tail(&matched_mods, &root_entry->matches);
            }

            /* Dynamic array reallocation based on args_nr */
            char **new_args = realloc(
                current_mod->args, (current_mod->args_nr + 1) * sizeof(char *));
            if (!new_args) {
                pr_error("Failed to create an argument");
                print_red(
                    stderr,
                    "Internal error while parsing option(s) '%s' failed",
                    arg);
                goto out;
            }
            current_mod->args = new_args;
            current_mod->args[current_mod->args_nr++] = (char *)arg;

            continue;
        }

        /* Module search and matching */
        module_entry_t *found = search_module_by_name(current_sub_modules, arg);
#ifdef CLI_ENABLE_MODULES_CHAINING
        if (!found && current_sub_modules != &root_sub_modules) {
            current_sub_modules = &root_sub_modules;
            found = search_module_by_name(current_sub_modules, arg);
        }
#endif
        if (!found) {
            pr_error("Module or submodule '%s' unknown in this context", arg);
            pr_module_short_usage(stderr, prog_name, &current_mod->mod);
            goto out;
        }

        if (found->matched) {
            pr_error("Module '%s' has already been specified", arg);
            pr_module_short_usage(stderr, prog_name, &found->mod);
            goto out;
        }

        found->matched = true;
        current_mod = found;

        list_add_tail(&matched_mods, &found->matches);
        current_sub_modules = &found->sub_modules;
    }

    pr_info("Loading specified modules completed successfully");

#ifdef DEBUG
    /* Print execution details */
    list_node_t *curr_node;
    module_entry_t *e;
    list_for_each(curr_node, &matched_mods)
    {
        e = container_of(curr_node, module_entry_t, matches);
        pr_debug(
            "Module: <%s> [level='%u', '%u' option(s)]",
            e->mod.name,
            e->level,
            e->args_nr);
        for (unsigned int i = 0; i < e->args_nr; i++) {
            pr_debug("%s", e->args[i]);
        }
    }
#endif

    ret = 0;

out:
    return ret;
}

#define printable(ch) (isprint((unsigned char)(ch)) ? (ch) : '#')

/**
 * @brief Parses options for a module using getopt_long.
 */
static int parse_cli_modules_options(
    int argc,
    char *const *argv,
    const char *optstring,
    const struct option *long_options,
    module_t *m)
{
    if (!argv || argc <= 0)
        return 0;

    char **parse_argv = malloc((size_t)(argc + 1) * sizeof(char *));
    if (!parse_argv) {
        pr_error("Allocation memory failed");
        print_red(stderr, "Internal error while parsing comand line option(s)");
        return -1;
    }

    parse_argv[0] = (m && m->name) ? (char *)m->name : "module";
    for (int i = 0; i < argc; i++) {
        parse_argv[i + 1] = (char *)argv[i];
    }
    int parse_argc = argc + 1;

    /* Complete reset of getopt internal state (glibc-specific) */
    optind = 0;
    opterr = 0; /* Disable default getopt error reporting to handle errors
                   explicitly */

    int opt;
    int long_idx = -1;
    int ret = 0;

    while ((opt = getopt_long(
                parse_argc, parse_argv, optstring, long_options, &long_idx)) !=
           -1) {
        pr_debug(
            "parsed opt=%d ('%c'), optind=%d", opt, printable(opt), optind);

        if (opt == '?') {
            print_red(
                stderr,
                "Error: Unrecognized option ('-%c')\n",
                printable(optopt));
            if (m && m->usage)
                pr_module_usage(stderr, prog_name, m);
            ret = -1;
            goto out;
        }

        if (opt == ':') {
            print_red(
                stderr,
                "Error: Option requires an argument: '-%c'\n",
                printable(optopt));
            if (m && m->usage)
                pr_module_usage(stderr, prog_name, m);
            ret = -1;
            goto out;
        }

        /* Match corresponding option in m->options */
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

        if (matched) {
            matched->is_set = true;
            matched->arg_val = optarg;
            pr_debug(
                "matched option '--%s' (-%c), val='%s'",
                matched->l_opt ? matched->l_opt : "",
                matched->s_opt ? matched->s_opt : ' ',
                matched->arg_val ? matched->arg_val : "none");
        } else {
            pr_error("unhandled option code: %d", opt);
            pr_module_usage(stderr, prog_name, m);
            ret = -1;
            goto out;
        }

        long_idx = -1;
    }

    pr_debug(
        "Parsing completed successfully for module: %s",
        m->name ? m->name : "unknown");

out:
    free(parse_argv);
    return ret;
}

/**
 * @brief Prepares optstring and long_options structures, then initiates
 * parsing.
 */
static int cli_parsing(void)
{
    list_node_t *curr_node;

    list_for_each(curr_node, &matched_mods)
    {
        module_entry_t *e = container_of(curr_node, module_entry_t, matches);
        module_t *m = &e->mod;

        if (!m || m->opt_nr == 0 || e->args_nr == 0)
            continue;

        char optstring[m->opt_nr * 3 + 2];
        struct option long_options[m->opt_nr + 1];
        size_t optstr_idx = 0;

        /* Leading colon to distinguish between an unknown option ('?') and a
         * missing argument (':') */
        optstring[optstr_idx++] = ':';

        for (size_t i = 0; i < m->opt_nr; i++) {

            module_option_t *o = m->options[i];

            /* Populate the long_options entry */
            long_options[i].name = o->l_opt;
            long_options[i].has_arg = o->has_arg;
            long_options[i].flag = NULL;
            long_options[i].val =
                o->s_opt ? (int)(unsigned char)o->s_opt : (int)(1000 + i);

            /* Populate optstring for short options */
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

        /* Sentinel for the end of the long_options array */
        long_options[m->opt_nr] = (struct option){0};
        optstring[optstr_idx] = '\0';

        pr_debug(
            "Generated optstring='%s' for %zu options in module '%s'",
            optstring,
            (size_t)m->opt_nr,
            m->name);

        if (parse_cli_modules_options(
                (int)e->args_nr, e->args, optstring, long_options, m) != 0) {
            return -1;
        }

        module_option_t **matched_opts =
            malloc(m->opt_nr * sizeof(module_option_t *));
        if (!matched_opts) {
            pr_error("Failed to allocate matched options array");
            print_red(
                stderr, "Internal error while parsing comand line option(s)");
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
 * @brief Runs the full CLI pipeline: match modules/arguments, then dispatch
 * options and action handlers, then release every module resource exactly
 * once.
 *
 * @param[in] argv Array of command-line argument strings (NULL-terminated).
 *
 * @return 0 on success, -1 on error.
 */
static int parse_cli(char *const *argv)
{
    int ret = -1;

    if (!argv && !*argv)
        return ret;
    prog_name = argv[0];

    if (parse_cli_modules(argv) != 0)
        goto out;

    if (cli_parsing() != 0)
        goto out;

    ret = 0;

out:
    cleanup_cli_modules();

    if (ret != 0)
        pr_error("Failed to parse comand line option(s)");

    return ret;
}

int launch_cli(char *const *argv)
{
    return parse_cli(argv);
}

/**
 * @brief Renders CLI usage instructions for a module.
 *
 * @param[in] stream Output stream (defaults to stderr if NULL).
 * @param[in] prog_name Executable name or path
 * @param[in] m Pointer to the module structure containing options and metadata.
 *
 * @return 0 on success, -1 if the module pointer is invalid.
 */
static void
module_usage_impl(FILE *stream, const char *progr_name, const module_t *m)
{
    print_yellow(stream, "Usage: %s %s \n\n", progr_name, m->usage);

    if (!m->options || m->opt_nr == 0) {
        return;
    }

    fprintf(stream, "Options:\n");

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

int pr_module_usage(FILE *stream, const char *progr_name, const module_t *m)
{
    const char *name;

    if (!m)
        return -1;

    if (!stream)
        stream = stderr;

    if (!progr_name)
        prog_name = " ";

    name = strrchr(prog_name, '/');
    if (name != NULL)
        name++;
    else
        name = prog_name;

    module_usage_impl(stream, name, m);
    return 0;
}

void pr_module_short_usage(
    FILE *stream, const char *progr_name, const module_t *m)
{
    const char *name;

    if (!m)
        return;

    if (!stream)
        stream = stderr;

    if (!progr_name)
        progr_name = " ";

    name = strrchr(progr_name, '/');
    if (name != NULL)
        name++;
    else
        name = progr_name;

    /* Highlight in red for error streams (stderr), yellow otherwise */
    if (stream == stderr) {
        print_red(
            stream,
            "Usage: %s %s %s\n",
            name,
            m->name ? m->name : "module",
            m->usage ? m->usage : "[OPTIONS]");
    } else {
        print_yellow(
            stream,
            "Usage: %s %s %s\n",
            name,
            m->name ? m->name : "module",
            m->usage ? m->usage : "[OPTIONS]");
    }
}