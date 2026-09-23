#!/bin/sh
#
# SPDX-License-Identifier: GPL-2.0
#
# vim: set ts=8 sw=8 noet tw=80 cc=80 fo+=t :
#
# ====================================================================
#         KDGB.SH - Linux Kernel Debugging Environment Setup
# ====================================================================
#
# NAME
#    kgdb - Configures a Linux kernel debugging environment.
#
# SYNOPSIS
#    kgdb [OPTIONS] [COMMAND]
#
# DESCRIPTION
#    kgdb automates the setup and maintenance of a Linux kernel
#    debugging environment. It manages the installation and uninstallation
#    of required packages, configures shell aliases, and tunes kernel
#    and system parameters.
#
#TODO: resync on the usage options
# OPTIONS
#    -h, --help                 Display this help message and exit.
#    -v, --verbose              Enable verbose output mode.
#    -p, --packages             List debugging packages.
#    -r, --remove               Remove packages.
#    -l, --list                 list items(packages or files, etc...).
#    -i, --install              Install packages.
#    -s, --set-files            Tune files for debugging.
#    -u, --unset-files          unset files configuration.
#    -k, --kernel-dir <dir>     Kernel source directory.
#
# EXIT STATUS
#    0    Success.
#    1    General error or invalid argument.
#    2    Permission denied (root required).
#
# WARNINGS
#    - Requires root privileges (sudo).
#    - Modifies system configuration files (e.g., ~/.bashrc, /etc/sysctl.conf, /proc).
#    - Alters system package state via the package manager (apt).
#
# AUTHORS
#    Midnight Walker <https://github.com/The-midnight-walker>
#
# LICENSE
#    GPL-2.0-or-later <https://www.gnu.org/licenses/gpl-2.0.html>
#

SCRIPT_NAME=$(basename -- "$0")
SCRIPT_DIR=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd) || exit 1
KFX_ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd) || exit 1

UTILS_SH="${SCRIPT_DIR}/utils.sh"

if [ ! -r "${UTILS_SH}" ]; then
	print_r '%s: cannot read %s\n' "${SCRIPT_NAME}" "${UTILS_SH}" >&2
	exit 1
fi

export KFX_ROOT SCRIPT_DIR

# shellcheck disable=SC1090
. "${UTILS_SH}"

usage() {
	print_y "Usage: debug [options]"
	echo "  -h, --help                 display this help message and exit"
	echo "  -v, --verbose              enable verbose output mode"
	echo "  -p, --packages             list debugging packages"
	echo "  -r, --remove               remove packages"
    echo "  -l, --list                 list items(packages or files, etc...)"
	echo "  -i, --install              install packages"
	echo "  -s, --set-files            tune files for debugging"
	echo "  -k, --kernel-dir <dir>     kernel source directory"
    echo "  -u, --unset-files          unset files configuration"
	return 0
}

# ===================
# PACKAGES MANAGEMENT
# ===================
# "$pkg_debug" : Debug packages retrieved from configuration file.
# Provided by the load_config function, which is called in main

_install_packages() {

	# shellcheck source=assets/scripts/parser.sh
	# shellcheck disable=SC2154
	if ! IS_INSTALLED_PACKAGES "${pkg_debug}" "i"; then
		print_g "---| no debugging packages to install found"
		return 0
	fi

	INSTALL_PACKAGES
}

_remove_packages() {

	if ! IS_INSTALLED_PACKAGES "${pkg_debug}" "r"; then
		print_g "---| no debugging packages to remove found"
		return 0
	fi

	REMOVE_PACKAGES
}

_list_packages() {
	IS_INSTALLED_PACKAGES "${pkg_debug}" "u"
}

# ==============
# FILES HANDLING
# ==============

# =======
# ALIASES
# =======

main() {
	if [ "$#" -eq 0 ]; then
		usage >&2
		return 0
	fi

	while [ "$#" -gt 0 ]; do
		case "$1" in
		-h | --help)
			usage
			return 0
			;;

		-v | --verbose)
			DO_VERBOSE=1
			shift
			;;

		-l | --list)
			# shellcheck disable=SC2034
			DO_LIST=1
			DO_VERBOSE=1
			shift
			;;

		-r | --remove)
			DO_REMOVE=1
			shift
			;;

		-i | --install)
			DO_INSTALL=1
			shift
			;;

		-s | --set)
			#DO_SET=1
			shift
			;;

		-u | --unset)
			#DO_UNSET=1
			shift
			;;

		-p | --packages)
			DO_PACKAGES=1
			shift
			;;

		-f | --files)
			DO_SETFILES=1
			shift
			;;

		-k | --kernel-dir)
			case "${2:-}" in
			'' | -*)
				print_err "Error: Option '$1' requires an argument" >&2
				return 1
				;;
			esac
			KERNEL_DIR="$2"
			shift 2
			;;

		--)
			shift
			break
			;;

		*)
			print_err "Error: Unknown option '$1'" >&2
			return 1
			;;
		esac
	done

	export DO_VERBOSE KERNEL_DIR

	if [ "${DO_PACKAGES:-0}" -eq 0 ] && [ "${DO_SETFILES:-0}" -eq 0 ] && [ "${DO_LIST:-0}" -eq 0 ]; then
		print_err "Error: nothing to do, expected -p , -l and/or -s" >&2
        usage
		return 1
	fi

	# Both actions below touch system state: refuse to go further as a
	# regular user rather than failing halfway through.
	IS_LOGIN_ROOT

	load_config || return 1

	# handle packages options
	if [ "${DO_PACKAGES:-0}" -eq 1 ]; then
		if [ "${DO_REMOVE:-0}" -eq 1 ] && [ "${DO_INSTALL:-0}" -eq 1 ]; then
			print_err "mutually exclusive options: --install and --remove"
			usage
			return 2
		fi

		# if DO_PACKAGES is set with or  not DO_LIST
		if [ "${DO_LIST:-0}" -eq 1 ]; then
			_list_packages|| return 1
        else
            if [ "${DO_REMOVE:-0}" -eq 1 ]; then
                _remove_packages || return 1
            fi

            if [ "${DO_INSTALL:-0}" -eq 1 ]; then
                _install_packages || return 1
            fi
		fi
	fi

	return 0
}

main "$@"
