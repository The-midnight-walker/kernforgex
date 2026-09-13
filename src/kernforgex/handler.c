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

extern char **envrion;

#define IS_OPT_SET(opt, s_opt) ((opt.is_set)) ? (s_opt) : ' '
#define eval_str(str, def) (str) ? str : def

void usage_impl(FILE *stream, const char *prog_name)
{
    fprintf(
        stream,
        "Usage: %s [OPTIONS]\n\n"
        "Options:\n"
        "  -v, --verbose    Enable verbose mode\n"
        "  -h, --help       Display this help\n",
        prog_name);
}

int usage(FILE *stream, const char *prog_name)
{
    const char *name;

    if (!stream) {
        pr_error("stream=%p", (void *)stream);
        return -1;
    }

    if (!prog_name) {
        pr_error("prog_name='%p'", (void *)prog_name);
        return -1;
    }

    /* get basename from the program name */
    name = strrchr(prog_name, '/');
    if (name != NULL)
        name++;
    else
        name = prog_name;

    if (stdout == stream || stderr == stream) {
        usage_impl(stream, (name) ? name : prog_name);
        return 0;
    }

    pr_error("invalid stream");
    return -1;
}

int init_cli_config(struct cli_config_struct *cfg)
{
    pr_debug("init command line structure configuration");
    if (!cfg) {
        pr_error("cli_cfg=%p", (void *)cfg);
        return -1;
    }

    INIT_FLAG(&cfg->help, HELP_L_OPT, no_argument, HELP_S_OPT);
    INIT_FLAG(&cfg->verbose, VERBOSE_L_OPT, no_argument, VERBOSE_S_OPT);
    INIT_FLAG(&cfg->kern_dbg, KERN_DBG_L_OPT, no_argument, KERN_DBG_S_OPT);
    INIT_FLAG(&cfg->remove, RM_L_OPT, no_argument, RM_S_OPT);
    INIT_FLAG(&cfg->packages, PKG_L_OPT, no_argument, PKG_S_OPT);
    INIT_FLAG(&cfg->vimrc, VIMRC_L_OPT, no_argument, VIMRC_S_OPT);
    INIT_FLAG(&cfg->makefile, MAKEFILE_L_OPT, no_argument, MAKEFILE_S_OPT);
    INIT_FLAG(&cfg->files, FILES_L_OPT, no_argument, FILES_S_OPT);

    return 0;
}

int clean_cli_cfg(struct cli_config_struct *cfg)
{
    pr_debug("clean command line struct configuration");
    if (!cfg) {
        pr_error("cli_cfg=%p", (void *)cfg);
        return -1;
    }

    return 0;
}

int handle(struct cli_config_struct *cfg)
{
    int ret = -1;

    pr_debug("handling command line");
    if (!cfg) {
        pr_error("cli_cfg=%p", (void *)cfg);
        return ret;
    }

    /* usage for help */
    if (cfg->help.is_set) {
        ret = usage(stdout, "");
        return 0;
    }

    /* debug kernel */
    if (cfg->kern_dbg.is_set)
        ret = debug_kernel_handle(
            (char *const[]){
                GET_FLAG_NAME(cfg->remove),
                GET_FLAG_NAME(cfg->verbose),
                GET_FLAG_NAME(cfg->packages),
                GET_FLAG_NAME(cfg->files),
                GET_FLAG_NAME(cfg->aliases),
            },
            0);

    if (cfg->vimrc.is_set)
        ret = 1;

    return ret;
}

int handle(struct cli_ctx *ctx)
{
    pr_debug("handling command line");
    if (check_cli_ctx(ctx)) {
        pr_error("invalid command line context arguments passed");
        return -1;
    }

    return handle_impl(ctx);
}

int debug_kernel_handle(char *const argv[], [[maybe_unused]] void *data)
{
    if (data) {
        pr_debug("passed a cookie");
    } else {
        pr_debug("no cookie passed, data=%p", (void *)data);
    }

    if (!argv)
        pr_debug("args=%p", argv);

    if (check_script_pathname(KERN_DBG_SH_PATH))
        return -1;

    pr_info(
        "debug kernel handler shell script pathanme is '%s'", KERN_DBG_SH_PATH);
    return execve_shell_script(KERN_DBG_SH_PATH, argv);
}

static int execve_shell_script_impl(const char *pathname, char *const argv[])
{
    pid_t sh_pid;
    int status, cur_pid, ret;
    (void)pathname;
    (void)argv;

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
