// SPDX-License-Identifier: GPL-2.0
//
// vim: set ts=8 sw=8 noet tw=80 cc=80 fo+=t :

/**
 * @file      shell.c
 * @author    midnight walker
 * @brief     Shell scripts utilities.
 *
 * @version   0.1
 * @date      2026-10-2
 * @copyright GNU General Public License v2.0
 *
 */

#define prfx_fmt "shell: "

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "debug.h"
#include "shell.h"

extern char **environ;

#define pr_debug_env()                                                         \
    do {                                                                       \
        char **ep;                                                             \
        for (ep = environ; *ep != NULL; ep++)                                  \
            pr_debug("%s", *ep);                                               \
    } while (0)

/**
 * @brief Implementation of the shell script execution via fork and execve.
 *
 * @param pathname Path to the script to execute.
 * @param argv Argument vector for the script.
 * @return Exit status of the script, or -1 on failure.
 */
static int execve_shell_script_impl(const char *pathname, char *const argv[])
{
    pid_t sh_pid;
    int status, cur_pid, ret;

    ret = -1;
    switch ((sh_pid = fork())) {
    case -1: /* Error on fork creation */
        pr_error("fork process failed");
        return ret;

    case 0: /* Child process */
        cur_pid = getpid();
        pr_debug("[ %d ] on create child process", cur_pid);

        /* print the environment variables for debugging */
        /*pr_debug_env();*/

        pr_debug("[ %d ] script path: %s", cur_pid, pathname);

        pr_debug("[ %d ] execute the script by execve...", cur_pid);
        errno = 0;

        /* Execute the shell script */
        execve(pathname, argv, environ);

        /* If execve encounters an error */
        pr_error("error encountered with errno = %d", errno);
        pr_error("%s", strerror(errno));
        pr_error("[ %d ] child process end execution abnormally", cur_pid);

        /* CRITICAL: Use _exit instead of exit to avoid flushing parent buffers
         */
        _exit(-1);
        break;

    default: /* Parent or current process */
        cur_pid = getpid();
        pr_debug("[ %d ] child process created with success", cur_pid);
        do {
            if (waitpid(sh_pid, &status, WUNTRACED | WCONTINUED) == -1) {
                pr_error("[ %d ] wait action for child failed", cur_pid);
                if (ECHILD == errno)
                    pr_debug("[ %d ] no children to wait for", cur_pid);
                return ret;
            }

            if (WIFSTOPPED(status)) {
                pr_debug(
                    "[ %d ] child stopped by signal %d",
                    cur_pid,
                    WSTOPSIG(status));
            }
#ifdef WIFCONTINUED
            if (WIFCONTINUED(status)) {
                pr_debug(
                    "[ %d ] child process continue on signal SIGCONT", cur_pid);
            }
#endif
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));

        /* Analyze child process status */
        if ((WIFEXITED(status))) {
            ret = WEXITSTATUS(status);
            pr_debug(
                "[ %d ] child exited normally with status %d", cur_pid, ret);
        }
        if (WIFSIGNALED(status)) {
#ifdef WCOREDUMP
            if (WCOREDUMP(status)) {
                pr_debug(
                    "[ %d ] child exited on signal %d with core dump generated",
                    cur_pid,
                    WTERMSIG(status));
            } else
#endif
            {
                pr_debug(
                    "[ %d ] child exited on signal %d",
                    cur_pid,
                    WTERMSIG(status));
            }
        }
        break;
    }
    pr_debug("execute shell script end; ret=%d", ret);
    return ret;
}

/**
 * @brief Wrapper function to validate arguments before executing the shell
 * script.
 * @param pathname Path to the script.
 * @param argv Argument vector.
 * @return Result status code.
 */
int execve_shell_script(const char *pathname, char *const argv[])
{
    if (!pathname) {
        pr_error(
            "script to execute path hasn't been specified script_path=%p",
            (void *)pathname);
        return -1;
    }

    /* Fallback mechanism if argv is NULL to prevent execve crash */
    char *default_argv[] = {(char *)pathname, NULL};
    char *const *effective_argv = argv ? argv : default_argv;

    if (!argv) {
        pr_debug(
            "args was NULL, falling back to default argv={pathname, NULL}");
    }

    pr_debug("execute shell script");
    return execve_shell_script_impl(pathname, (char *const *)effective_argv);
}
