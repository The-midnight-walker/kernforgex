#!/bin/sh
#
# SPDX-License-Identifier: GPL-2.0
#
# vim: set ts=8 sw=8 noet tw=80 cc=80 fo+=t :
#
#
# ========================================================
#         PARSER.SH - Configuration File Parser
# ========================================================
#
# NAME
#    parser.sh - Parses configuration files for Uranus project.
#
# SYNOPSIS
#    parser.sh [CONFIG_FILE]
#
# DESCRIPTION
#    parser.sh parses a configuration file and outputs
#    POSIX shell-compatible variable assignments to be evaluated by
#    the calling shell environment.
#
# EXIT STATUS
#    0    Success.
#    1    Config file missing or invalid argument.
#
# AUTHORS
#    Midnight Walker <https://github.com/The-midnight-walker>
#
# LICENSE
#    GPL-2.0-or-later <https://www.gnu.org/licenses/gpl-2.0.html>
#

parser() {
	config_file="$1"
	if [ ! -f "${config_file}" ]; then
		print_err "Config file '${config_file}' not found." >&2
		return 1
	fi

	pkg_build=""
	pkg_debug=""
	pkg_misc=""
	section=""

	sys_panic_on_oops=""
	sys_sysrq_mask=""
	sys_reboot_after_panic=""

	alias_kdir=""
	alias_dmesg=""
	alias_decode_strace=""
	alias_checkpatch=""

	while IFS= read -r line || [ -n "${line}" ]; do
		# Strip comments and surrounding whitespace
		line="${line%%#*}"
		line=$(printf '%s' "${line}" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
		[ -z "${line}" ] && continue

		case "${line}" in
		\[*\])
			section="${line#\[}"
			section="${section%\]}"
			continue
			;;
		esac

		case "${section}" in
		system.debug)
			key="${line%%=*}"
			val="${line#*=}"
			key=$(printf '%s' "${key}" | sed 's/[[:space:]]*$//')
			val=$(printf '%s' "${val}" | sed 's/^[[:space:]]*//')
			case "${key}" in
			panic_on_oops)
				case "${val}" in
				0 | 1) sys_panic_on_oops="${val}" ;;
				*)
					print_err "Invalid panic_on_oops: '${val}' (expected 0 or 1)" >&2
					return 1
					;;
				esac
				;;
			sysrq_mask)
				sys_sysrq_mask="${val}"
				;;
			reboot_after_panic)
				case "${val}" in
				'' | *[!0-9]*)
					print_err "Invalid reboot_after_panic: '${val}' (expected an integer)" >&2
					return 1
					;;
				*) sys_reboot_after_panic="${val}" ;;
				esac
				;;
			esac
			;;

		packages.build) pkg_build="${pkg_build} ${line}" ;;
		packages.debug) pkg_debug="${pkg_debug} ${line}" ;;
		packages.misc) pkg_misc="${pkg_misc} ${line}" ;;

		aliases.misc)
			key="${line%%=*}"
			val="${line#*=}"
			key=$(printf '%s' "${key}" | sed 's/[[:space:]]*$//')
			val=$(printf '%s' "${val}" | sed 's/^[[:space:]]*//')
			case "${key}" in
			kdir) alias_kdir="${val}" ;;
			dmesg) alias_dmesg="${val}" ;;
			decode-strace) alias_decode_strace="${val}" ;;
			checkpatch) alias_checkpatch="${val}" ;;
			esac
			;;
		esac
	done <"${config_file}"

	# Clean leading spaces from lists
	pkg_build="${pkg_build# }"
	pkg_debug="${pkg_debug# }"
	pkg_misc="${pkg_misc# }"

	export sys_panic_on_oops sys_sysrq_mask sys_reboot_after_panic
	export pkg_build pkg_debug pkg_misc
	export alias_kdir alias_dmesg alias_decode_strace alias_checkpatch
}
