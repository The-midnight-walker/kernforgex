#!/bin/sh
# shellcheck disable=SC2154
#
# SPDX-License-Identifier: GPL-2.0
#
# vim: set ts=8 sw=8 noet tw=80 cc=80 fo+=t :
#
# ====================================================================
#         KERNDEBUG.SH - Linux Kernel Debugging Environment Setup
# ====================================================================
#
# NAME
#    kerndebug - Configures a Linux kernel debugging environment.
#
# SYNOPSIS
#    kerndebug [OPTIONS] [COMMAND]
#
# DESCRIPTION
#    kerndebug automates the setup and maintenance of a Linux kernel
#    debugging environment. It manages the installation and uninstallation
#    of required packages  and tunes kernel and system parameters.
#
#TODO: resync on the usage options
# OPTIONS
#    -h, --help                 Display this help message and exit.
#    -v, --verbose              Enable verbose output mode.
#
# EXIT STATUS
#   0    Success
#   1    General error
#   2    Invalid argument / usage error
#
# WARNINGS
#    - Requires root privileges (sudo).
#    - Alters system package state via the package manager (apt).
#
# AUTHORS
#    Midnight Walker <https://github.com/The-midnight-walker>
#
# LICENSE
#    GPL-2.0-or-later <https://www.gnu.org/licenses/gpl-2.0.html>
#

# locate script directory and root of the project
SCRIPT_NAME=$(basename -- "$0")
SCRIPT_DIR=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd) || exit 1
# shellcheck disable=SC2034
KFGX_ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd) || exit 1

UTILS_SH="${SCRIPT_DIR}/utils.sh"

if [ ! -r "${UTILS_SH}" ]; then
	printf '%s: cannot read %s\n' "${SCRIPT_NAME}" "${UTILS_SH}" >&2
	exit 1
fi

# shellcheck disable=SC1090
. "${UTILS_SH}"

# ==========================================================================
# @brief Parse the given configuration file and load the relevant variables.
#
# @details This function reads the specified configuration file and extracts
#          the necessary variables for kernel debugging setup. It populates
#          the global variables used by other functions in this script.
#
# @param config_file The path to the configuration file to be parsed.
# @param action The action to be performed (install, remove, list).
# @return 0 on success, 1 if the configuration file is missing or invalid.
# ==========================================================================
kerndebug_load_config() {
	local_config_file="$1"
	local_action="$2"
	local_section=""

	if [ ! -f "${local_config_file}" ]; then
		print_err "Config file '${local_config_file}' not found." >&2
		return 1
	fi

	if [ -z "${local_action}" ]; then
		print_err "Action parameter is missing. Please specify an action (install, remove, list)." >&2
		return 1
	fi

	# Retrieve the packages from the configuration file based on the action
	if [ "${local_action}" = "p" ]; then
		pkg_kdebug=""

		while IFS= read -r local_line || [ -n "${local_line}" ]; do
			# Strip comments and surrounding whitespace
			local_line="${local_line%%#*}"
			local_line=$(printf '%s' "${local_line}" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
			[ -z "${local_line}" ] && continue

			case "${local_line}" in
			\[*\])
				local_section="${local_line#\[}"
				local_section="${local_section%\]}"
				continue
				;;
			esac

			if [ "${local_section}" = "packages.debug" ]; then
				pkg_kdebug="${pkg_kdebug} ${local_line}"
			fi
		done <"${local_config_file}"
	fi

	# Clean leading and trailing whitespaces properly
	pkg_kdebug=$(printf '%s' "${pkg_kdebug}" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
}

# ===================
# PACKAGES MANAGEMENT
# ===================
# "$pkg_kdebug" : Debug packages retrieved from configuration file.
# "$CONF_FILE" : Configuration file path provided from sourcing the utils.sh script.
# Provided by the load_config function, which is called in main

_load_packages_config() {
	# shellcheck source=kernforgex/scripts/utils.sh
	kerndebug_load_config "${CONF_FILE}" "p" || return 1

	print_info "--- current kernel debugging packages status in your environment ---"
}

_install_packages() {

	_load_packages_config || return 1
	if ! IS_INSTALLED_PACKAGES "${pkg_kdebug}" "i"; then
		print_g "---| no debugging packages to install found"
		return 0
	fi

	INSTALL_PACKAGES
}

_remove_packages() {

	_load_packages_config || return 1
	if ! IS_INSTALLED_PACKAGES "${pkg_kdebug}" "r"; then
		print_g "---| no debugging packages to remove found"
		return 0
	fi

	REMOVE_PACKAGES
}

_list_packages() {
	_load_packages_config || return 1

	IS_INSTALLED_PACKAGES "${pkg_kdebug}" "i"
}

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

		-p | --packages)
			DO_PACKAGES=1
			shift
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

	export DO_VERBOSE

	if [ "${DO_PACKAGES:-0}" -eq 0 ]; then
		print_err "Error: bad usage" >&2
		return 2
	fi

	# Both actions below touch system state: refuse to go further as a
	# regular user rather than failing halfway through.

	# handle packages options
	if [ "${DO_PACKAGES:-0}" -eq 1 ]; then
		if [ "${DO_REMOVE:-0}" -eq 1 ] && [ "${DO_INSTALL:-0}" -eq 1 ]; then
			print_err "mutually exclusive options: --install and --remove"
			return 2
		fi

		if [ "${DO_LIST:-0}" -eq 1 ]; then
			_list_packages || return 1
		elif [ "${DO_REMOVE:-0}" -eq 1 ]; then
			IS_LOGIN_ROOT
			_remove_packages || return 1
		elif [ "${DO_INSTALL:-0}" -eq 1 ]; then
			IS_LOGIN_ROOT
			_install_packages || return 1
		else
			DO_LIST=1
			DO_VERBOSE=1
			_list_packages || return 1
		fi

		return 0
	fi
	return 0
}

main "$@"
