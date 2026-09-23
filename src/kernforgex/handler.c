// SPDX-License-Identifier: GPL-2.0
//
// vim: set ts=8 sw=8 noet tw=80 cc=80 fo+=t :

/**
 * @file      handler.c
 * @author    midnight walker
 * @brief     Execute options and arguments with handlers.
 * @version   0.1
 * @date      2026-09-06
 * @copyright GNU General Public License v2.0
 */

#define pr_prfx "handle: "

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "clicntl.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;
static int debug_kernel_handle(struct cli_ctx *, struct cli_opt *);

/* Default declarative options table */
static const cli_opt_t default_cli_options[OPT_COUNT] = {
    [OPT_HELP] =
        {
            .id = OPT_HELP,
            .s_opt = 'h',
            .l_opt = "help",
            .has_arg = no_argument,
            .arg_name = NULL,
            .desc = "Display this help message and exit",
            .action = NULL,
        },
    [OPT_VERBOSE] =
        {
            .id = OPT_VERBOSE,
            .s_opt = 'v',
            .l_opt = "verbose",
            .has_arg = no_argument,
            .arg_name = NULL,
            .desc = "Enable verbose output mode",
            .action = NULL,
        },
    [OPT_KERN_DBG] =
        {
            .id = OPT_KERN_DBG,
            .s_opt = 'd',
            .l_opt = "kernel-debug",
            .has_arg = no_argument,
            .arg_name = NULL,
            .desc = "Configure Linux kernel debugging environment",
            .action = debug_kernel_handle,
        },
    [OPT_PACKAGES] =
        {
            .id = OPT_PACKAGES,
            .s_opt = 'p',
            .l_opt = "packages",
            .has_arg = no_argument,
            .arg_name = NULL,
            .desc = "Install required packages (list with -l or remove with -r)",
            .action = NULL,
        },
    [OPT_REMOVE] =
        {
            .id = OPT_REMOVE,
            .s_opt = 'r',
            .l_opt = "remove",
            .has_arg = no_argument,
            .arg_name = NULL,
            .desc = "With -p, remove packages instead of installing",
            .action = NULL,
        },
    [OPT_INSTALL] =
        {
            .id = OPT_INSTALL,
            .s_opt = 'i',
            .l_opt = "install",
            .has_arg = no_argument,
            .arg_name = NULL,
            .desc = "With -i, installing packages",
            .action = NULL,
        },
    [OPT_ALIASES] =
        {
            .id = OPT_ALIASES,
            .s_opt = 'a',
            .l_opt = "aliases",
            .has_arg = no_argument,
            .arg_name = NULL,
            .desc = "Configure shell aliases",
            .action = NULL,
        },
    [OPT_LIST] =
        {
            .id = OPT_LIST,
            .s_opt = 'l',
            .l_opt = "list",
            .has_arg = no_argument,
            .arg_name = NULL,
            .desc = "list pakcages, files , ... (on verbose)",
            .action = NULL,
        },
    [OPT_FILES] =
        {
            .id = OPT_FILES,
            .s_opt = 'f',
            .l_opt = "files",
            .has_arg = no_argument,
            .arg_name = NULL,
            .desc = "Tune files",
            .action = NULL,
        },
    [OPT_VIMRC] =
        {
            .id = OPT_VIMRC,
            .s_opt = 'c',
            .l_opt = "vimrc",
            .has_arg = no_argument,
            .arg_name = NULL,
            .desc = "Deploy a specific util vim configuration",
            .action = NULL,
        },
    [OPT_MAKEFILE] =
        {
            .id = OPT_MAKEFILE,
            .s_opt = 'm',
            .l_opt = "makefile",
            .has_arg = no_argument,
            .arg_name = NULL,
            .desc = "Deploy module Makefile template",
            .action = NULL,
        },
};

int cli_ctx_init(struct cli_ctx *ctx, int argc, char *const *argv)
{
    if (!ctx) {
        pr_error("ctx is NULL");
        return -1;
    }

    ctx->argc = argc;
    ctx->argv = argv;
    ctx->opts_count = OPT_COUNT;
    memcpy(ctx->opts, default_cli_options, sizeof(default_cli_options));

    return 0;
}

static void
usage_impl(FILE *stream, const char *prog_name, const struct cli_ctx *ctx)
{
    const cli_opt_t *opts;
    size_t count;

    print_yellow(stream, "Usage: %s [OPTIONS]\n\n", prog_name);
    print_yellow(
        stream, "Linux kernel and drivers development orchestration CLI.\n\n");
    fprintf(stream, "Options:\n");

    opts = ctx ? ctx->opts : default_cli_options;
    count = ctx ? ctx->opts_count : OPT_COUNT;

    for (size_t i = 0; i < count; i++) {
        const cli_opt_t *o = &opts[i];
        char opt_buf[64];

        if (o->s_opt && o->l_opt) {
            if (o->has_arg == required_argument)
                snprintf(
                    opt_buf,
                    sizeof(opt_buf),
                    "  -%c, --%s %s",
                    o->s_opt,
                    o->l_opt,
                    o->arg_name ? o->arg_name : "<val>");
            else
                snprintf(
                    opt_buf,
                    sizeof(opt_buf),
                    "  -%c, --%s",
                    o->s_opt,
                    o->l_opt);
        } else if (o->l_opt) {
            if (o->has_arg == required_argument)
                snprintf(
                    opt_buf,
                    sizeof(opt_buf),
                    "      --%s %s",
                    o->l_opt,
                    o->arg_name ? o->arg_name : "<val>");
            else
                snprintf(opt_buf, sizeof(opt_buf), "      --%s", o->l_opt);
        } else if (o->s_opt) {
            if (o->has_arg == required_argument)
                snprintf(
                    opt_buf,
                    sizeof(opt_buf),
                    "  -%c %s",
                    o->s_opt,
                    o->arg_name ? o->arg_name : "<val>");
            else
                snprintf(opt_buf, sizeof(opt_buf), "  -%c", o->s_opt);
        } else {
            continue;
        }

        fprintf(stream, "%-32s %s\n", opt_buf, o->desc ? o->desc : "");
    }
    fprintf(stream, "\n");
}

int usage(FILE *stream, const char *prog_name, const struct cli_ctx *ctx)
{
    const char *name;

    if (!stream)
        stream = stderr;

    // TODO: passed variable PROJECT_NAME via CMkakeLists.txt
    if (!prog_name)
        prog_name = "kfgx";

    name = strrchr(prog_name, '/');
    if (name != NULL)
        name++;
    else
        name = prog_name;

    usage_impl(stream, name, ctx);
    return 0;
}

static int
default_handle(struct cli_ctx *ctx, [[maybe_unused]] struct cli_opt *opt)
{
    /* If verbose mode is enabled, adjust log level */
    if (cli_has_flag(ctx, OPT_VERBOSE)) {
        enable_debug_loglevel(DBG_LOGLEVEL_DEBUG);
        pr_debug("verbose mode enabled");
    }

    /* If --help or no option is requested for the main program, display usage
     */
    return usage(stdout, ctx->argv[0], ctx);
}

static int
debug_kernel_handle(struct cli_ctx *ctx, [[maybe_unused]] struct cli_opt *opt)
{
    /* Construct arguments array forwarding only active flags */
    char *kdbg_argv[9];
    int idx = 0;

    if (check_script_pathname(KERN_DBG_SH_PATH))
        return -1;

    pr_info(
        "debug kernel handler shell script pathname is '%s'", KERN_DBG_SH_PATH);

    kdbg_argv[idx++] = (char *)KERN_DBG_SH_PATH;

    if (cli_has_flag(ctx, OPT_HELP))
        kdbg_argv[idx++] = "-h";

    if (cli_has_flag(ctx, OPT_VERBOSE))
        kdbg_argv[idx++] = "-v";

    if (cli_has_flag(ctx, OPT_INSTALL))
        kdbg_argv[idx++] = "-i";

    if (cli_has_flag(ctx, OPT_LIST))
        kdbg_argv[idx++] = "-l";

    if (cli_has_flag(ctx, OPT_PACKAGES))
        kdbg_argv[idx++] = "-p";

    if (cli_has_flag(ctx, OPT_REMOVE))
        kdbg_argv[idx++] = "-r";

    if (cli_has_flag(ctx, OPT_FILES))
        kdbg_argv[idx++] = "-s";

    kdbg_argv[idx] = NULL;

    return execve_shell_script(KERN_DBG_SH_PATH, kdbg_argv);
}

int handle(struct cli_ctx *ctx)
{
    bool any_exec = 0;
    int ret = 0;

    if (check_cli_ctx(ctx)) {
        pr_error("invalid command line context arguments passed");
        return -1;
    }

    pr_debug("handling command line");

    /* Execute registered actions */
    for (size_t i = 0; i < ctx->opts_count; i++) {
        cli_opt_t *opt = &ctx->opts[i];
        if (opt->is_set && opt->action) {
            any_exec = 1;
            pr_info(
                "running action for option '--%s'",
                opt->l_opt ? opt->l_opt : "");
            int res = opt->action(ctx, opt);
            if (res != 0)
                ret = res;
        }
    }

    if (any_exec)
        return ret;

    return default_handle(ctx, NULL);

    /* Handle configuration files deployment
    if (cli_has_flag(ctx, OPT_VIMRC)) {
        pr_info("deploying kernel-specific vimrc configuration...");
    }

    if (cli_has_flag(ctx, OPT_MAKEFILE)) {
        pr_info("deploying kernel module Makefile template...");
    }*/
}

static int execve_shell_script_impl(const char *pathname, char *const argv[])
{
    pid_t sh_pid;
    int status, cur_pid, ret;

#define pr_debug_env()                                                         \
    do {                                                                       \
        if (environ != NULL) {                                                 \
            for (char **ep = environ; *ep != NULL; ep++)                       \
                pr_debug("%s", *ep);                                           \
        }                                                                      \
    } while (0)

    ret = -1;
    switch ((sh_pid = fork())) {

    case -1: /*error on fork creation */
        pr_error("fork processus failed");
        return ret;

    case 0: /*child process */
        cur_pid = getpid();
        pr_info("[ %d ] on create child process", cur_pid);

        /* print child process environment variables */
        /*
        pr_debug("[ %d ] child process environment variables", cur_pid);
        pr_debug_env();
        */

        pr_info("[ %d ] execute the script by execve...", cur_pid);
        errno = 0;
        // execute the script
        execve(pathname, argv, environ);

        // the execve encounters an error
        pr_error("error encountered with errno = %d", errno);
        pr_error("%s", strerror(errno));
        pr_error("[ %d ] child process end execution abnormaly", cur_pid);
        exit(-1);
        break;

    default: /*parent or current process */
        cur_pid = getpid();
        pr_info("[ %d ] child process create with success", cur_pid);

        do {
            if (waitpid(sh_pid, &status, WUNTRACED | WCONTINUED) == -1) {
                pr_error("[ %d ] wait action for child failed", cur_pid);
                if (ECHILD == errno)
                    pr_info("[ %d ] no children to wait for", cur_pid);
                return ret;
            }

            /* print parent process environment variables */
            /*
            pr_debug("[ %d ] process environment variables", cur_pid);
            pr_debug_env();
            */

            if (WIFSTOPPED(status)) {
                pr_info(
                    "[ %d ] child stopped by signal %d",
                    cur_pid,
                    WSTOPSIG(status));
            }

#ifdef WIFCONTINUED
            if (WIFCONTINUED(status)) {
                pr_info(
                    "[ %d ] child process continue on signal SIGCONT", cur_pid);
            }
#endif
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));

        /* analyze child process status */
        if ((WIFEXITED(status))) {
            ret = WEXITSTATUS(status);
            pr_info("[ %d ] child exited normaly with status %d", cur_pid, ret);
        }

        if (WIFSIGNALED(status)) {
            pr_info(
                "[ %d ] child exited on signal %d "
#ifdef WCOREDUMP
                "with core dump generated"
#endif
                ,
                cur_pid,
                WTERMSIG(status));
        }
        break;
    }
    pr_debug("execute shell script end; ret=%d", ret);
    return ret;
}

int execve_shell_script(const char *pathname, char *const argv[])
{
    if (!pathname) {
        pr_error(
            "script to execute path didn't have been specified script_path=%p",
            (void *)pathname);
        return -1;
    }

    if (!argv)
        pr_debug("args=%p", (void *)argv);

    pr_debug("execute shell script");

    return execve_shell_script_impl(pathname, argv);
}
