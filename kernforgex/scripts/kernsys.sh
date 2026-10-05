#!/bin/sh
#
# SPDX-License-Identifier: GPL-2.0
#
# vim: set ts=8 sw=8 noet tw=80 cc=80 fo+=t :
#
# ==============================================================================
#        KERNSYS.SH - Linux Kernel & System Environment Setup
# ==============================================================================
#
# NAME
#    kernsys - Configures the Linux kernel, driver,
#              and system development environment.
#
# SYNOPSIS
#    kernsys [OPTIONS] [COMMAND]
#
# DESCRIPTION
#    kernsys automates the setup, and maintenance of the necessary
#    miscellaneous environment for Linux kernel development, driver writing,
#    and overall low-level system engineering. It manages required packages and
#    tunes system parameters.
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
# ==============================================================================

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

# ==============================================================================
# @brief Parse the given configuration file and load the relevant variables.
#
# @details This function reads the specified configuration file and extracts
#          the necessary variables. It populates the global variables used
#          by other functions in this script.
#
# @param config_file The path to the configuration file to be parsed.
# @param action The action to be performed (install, remove, list).
# @return 0 on success, 1 if the configuration file is missing or invalid.
# ==============================================================================

kernsys_load_config() {
    local_config_file="$1"
    local_action="$2"
    local_section=""

    # Check if configuration file exists
    if [ ! -f "${local_config_file}" ]; then
        print_err "Config file '${local_config_file}' not found." >&2
        return 1
    fi

    # Check if action parameter is provided
    if [ -z "${local_action}" ]; then
        print_err "Action parameter is missing. Please specify an action." >&2
        return 1
    fi

    # Single-pass loop through the configuration file
    while IFS= read -r local_line || [ -n "${local_line}" ]; do
        # Strip comments and surrounding whitespace
        local_line="${local_line%%#*}"
        local_line=$(printf '%s' "${local_line}" |
            sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
        [ -z "${local_line}" ] && continue

        # Handle section headers [section.name]
        case "${local_line}" in
        \[*\])
            local_section="${local_line#\[}"
            local_section="${local_section%\]}"
            continue
            ;;
        esac

        # Process packages action ("p") under [packages.system]
        if [ "${local_action}" = "p" ] && [ "${local_section}" = "packages.system" ]; then
            pkg_sys="${pkg_sys} ${local_line}"
        fi

        # Process system parameters action ("f") under [system.debug]
        if [ "${local_action}" = "p" ] || [ "${local_action}" = "f" ]; then
            : # keep parser stable
        fi

        if [ "${local_action}" = "f" ] && [ "${local_section}" = "system.files" ]; then
            case "${local_line}" in
            *=*)
                key="${local_line%%=*}"
                val="${local_line#*=}"

                # Clean leading/trailing spaces around key and value
                key=$(printf '%s' "${key}" | sed 's/[[:space:]]*$//;s/^[[:space:]]*//')
                val=$(printf '%s' "${val}" | sed 's/[[:space:]]*$//;s/^[[:space:]]*//')

                case "${key}" in
                panic_on_oops)
                    case "${val}" in
                    0 | 1) sys_panic_on_oops="${sys_panic_on_oops}${val}" ;;
                    *)
                        print_err "Invalid panic_on_oops: '${val}' (expected 0 or 1)" >&2
                        return 1
                        ;;
                    esac
                    ;;
                sysrq_mask)
                    sys_sysrq_mask="${sys_sysrq_mask}${val}"
                    ;;
                reboot_after_panic)
                    case "${val}" in
                    '' | *[!0-9]*)
                        print_err "Invalid reboot_after_panic: '${val}' (expected an integer)" >&2
                        return 1
                        ;;
                    *) sys_reboot_after_panic="${sys_reboot_after_panic}${val}" ;;
                    esac
                    ;;
                esac
                ;;
            esac
        fi
    done <"${local_config_file}"

    # Clean leading and trailing whitespaces for pkg_sys if action was "p"
    if [ "${local_action}" = "p" ]; then
        pkg_sys=$(printf '%s' "${pkg_sys}" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
    fi

    return 0
}

# ===================
# PACKAGES MANAGEMENT
# ===================

_load_packages_config() {
    pkg_sys=""
    kernsys_load_config "${CONF_FILE}" "p" || return 1
    print_info "Current core system packages status in your environment"
}

_install_packages() {
    _load_packages_config || return 1
    if ! IS_INSTALLED_PACKAGES "${pkg_sys}" "i"; then
        print_g "No core system packages to install found"
        return 0
    fi
    INSTALL_PACKAGES
}

_remove_packages() {
    _load_packages_config || return 1
    if ! IS_INSTALLED_PACKAGES "${pkg_sys}" "r"; then
        print_g "No core system to remove found"
        return 0
    fi
    REMOVE_PACKAGES
}

_list_packages() {
    _load_packages_config || return 1
    IS_INSTALLED_PACKAGES "${pkg_sys}" "i"
}

# ================================
# APPLY SYSTEM FILES CONFIGURATION
# ================================

_load_files_config() {
    sys_panic_on_oops="panic_on_oops::active panic on oops::/proc/sys/kernel/panic_on_oops::"
    sys_sysrq_mask="sysrq_mask::system request mask::/proc/sys/kernel/sysrq::"
    sys_reboot_after_panic="reboot_after_panic::reboot delay after panic::/proc/sys/kernel/panic::"

    kernsys_load_config "${CONF_FILE}" "f" || return 1
    return 0
}

_print_sys_row() {
    sys_entry="$1"
    [ -z "${sys_entry}" ] && return 0

    opt_name=$(printf '%s\n' "${sys_entry}" | awk -F'::' '{print $1}')
    desc=$(printf '%s\n' "${sys_entry}" | awk -F'::' '{print $2}')
    pathname=$(printf '%s\n' "${sys_entry}" | awk -F'::' '{print $3}')
    cfg_val=$(printf '%s\n' "${sys_entry}" | awk -F'::' '{print $4}')

    if [ -z "${cfg_val}" ]; then
        display_cfg="-"
    else
        display_cfg="${cfg_val}"
    fi

    if [ -f "${pathname}" ]; then
        current_val=$(tr -d '\n' <"${pathname}" 2>/dev/null)
        [ -z "${current_val}" ] && current_val="-"
    else
        current_val="N/A"
    fi

    printf "   %-22s  %-26s  %-32s    %-15s    %-15s\n" "${opt_name}" "${desc}" "${pathname}" "${display_cfg}" "${current_val}"
}

_list_files() {
    print_info "Current core system files configuration value in your environment"
    _load_files_config || return 1

    if [ "${DO_COLOR:-0}" -eq 1 ]; then
        printf "\033[32m   %-22s  %-26s  %-32s  %-15s  %-15s\n\033[0m" "option's name" "description" "file" "config value" "current value"
    else
        printf "   %-22s  %-26s  %-32s  %-15s  %-15s\n" "option's name" "description" "file" "config value" "current value"
    fi

    _print_sys_row "${sys_panic_on_oops}"
    _print_sys_row "${sys_sysrq_mask}"
    _print_sys_row "${sys_reboot_after_panic}"

    return 0
}

_apply_sys_row() {
    sys_entry="$1"
    [ -z "${sys_entry}" ] && return 0

    pathname=$(printf '%s\n' "${sys_entry}" | awk -F'::' '{print $3}')
    val=$(printf '%s\n' "${sys_entry}" | awk -F'::' '{print $4}')

    if [ -n "${val}" ] && [ -f "${pathname}" ]; then
        if printf '%s\n' "${val}" >"${pathname}" 2>/dev/null; then
            print_g "Successfully updated ${pathname} to ${val}"
        else
            print_err "Failed to write value '${val}' to ${pathname}" >&2
            return 1
        fi
    else
        print_err "Invalid path or empty value for entry: ${pathname}" >&2
        return 1
    fi
}

_set_files() {
    print_info "Applying core system files configuration values..."
    _load_files_config || return 1

    _apply_sys_row "${sys_panic_on_oops}" || return 1
    _apply_sys_row "${sys_sysrq_mask}" || return 1
    _apply_sys_row "${sys_reboot_after_panic}" || return 1

    return 0
}

main() {
    if [ "$#" -eq 0 ]; then
        return 2
    fi

    while [ "$#" -gt 0 ]; do
        case "$1" in
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
        -c | --color)
            DO_COLOR=1
            shift
            ;;
        -p | --packages)
            DO_PACKAGES=1
            shift
            ;;
        -f | --files)
            DO_FILES=1
            shift
            ;;
        -s | --set)
            DO_SET=1
            shift
            ;;
        --)
            shift
            break
            ;;
        *)
            print_err "unknown option '$1'" >&2
            return 2
            ;;
        esac
    done

    export DO_VERBOSE DO_COLOR
    load_ansi_color

    if [ "${DO_PACKAGES:-0}" -eq 0 ] && [ "${DO_FILES:-0}" -eq 0 ]; then
        print_err "bad usage" >&2
        return 2
    fi

    # handle packages options
    if [ "${DO_PACKAGES:-0}" -eq 1 ]; then
        if [ "${DO_REMOVE:-0}" -eq 1 ] && [ "${DO_INSTALL:-0}" -eq 1 ]; then
            print_err "mutually exclusive options: --install and --remove"
            return 2
        fi

        if [ "${DO_LIST:-0}" -eq 1 ]; then
            _list_packages || return 1
        elif [ "${DO_REMOVE:-0}" -eq 1 ]; then
            IS_LOGIN_ROOT || return 1
            _remove_packages || return 1
        elif [ "${DO_INSTALL:-0}" -eq 1 ]; then
            IS_LOGIN_ROOT || return 1
            _install_packages || return 1
        else
            if [ "${DO_SET:-0}" -eq 0 ]; then
                _list_packages || return 1
            fi
        fi
    fi

    # handle files management options
    if [ "${DO_FILES:-0}" -eq 1 ]; then
        if [ "${DO_SET:-0}" -eq 1 ]; then
            IS_LOGIN_ROOT || return 1
            _set_files || return 1
        elif [ "${DO_LIST:-0}" -eq 1 ]; then
            _list_files || return 1
        else
            _list_files || return 1
        fi
    fi

    return 0
}

main "$@"