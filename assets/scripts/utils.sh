#!/bin/sh
#
# SPDX-License-Identifier: GPL-2.0
#
# vim: set ts=8 sw=8 noet tw=80 cc=80 fo+=t :

# ====================
# CONFIGURATIONS FILES
# ====================
# NOTE: resolved relative to KFX_ROOT when the caller exports it (e.g. from
# kgdb.sh, which already computes its own SCRIPT_DIR). Falls back to the
# previous behaviour (relative to cwd) when unset, so nothing breaks for
# callers that don't set it.
CONF_FILE="${KFX_ROOT:-..}/configs/kernforgex.conf"
PARSER_SH="${KFX_ROOT:-..}/scripts/parser.sh"

. "$PARSER_SH"

#=============
# OUTPUT STYLE
#=============
# ANSI color definitions (POSIX printf compatible)
RED='\033[1;31m'
GREEN='\033[1;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

OK="${GREEN}[ ok ]${NC}"
NOT="${RED}[ X  ]${NC}"

print_y(){
    printf "%b%s%b\n" "$YELLOW" "$1" "$NC"
}

print_g(){
    printf "%b%s%b\n" "$GREEN" "$1" "$NC"
}

print_r(){
    printf "%b%s%b\n" "$RED" "$1" "$NC"
}

print_err() {
    # Print error message in red
    print_r "$1"
}

print_success() {
    # Print success message in green
    print_g "$1"

}

print_info() {
    # Print informational message in yellow only if verbose mode is enabled
    # NOTE: default changed 1 -> 0. VERBOSE is only ever set to 1 by -v in
    # kgdb.sh; with a default of 1 every info message printed unconditionally
    # and -v had no effect.
    if [ "${VERBOSE:-0}" -eq 1 ]; then
        print_y "$1"
    fi
}

#==============================
# RIGHTS, FILES AND PRIVILIEGES
#==============================

IS_LOGIN_ROOT() {
    # Verify that this script is running with root privileges
    # NOTE: return instead of exit. This function is sourced and called from
    # inside `if IS_INSTALLED_PACKAGES ...; then` contexts; an exit here
    # would terminate the caller's shell (including an interactive shell
    # this file was sourced into), not just the operation in progress.
    if [ "$(id -u)" -ne 0 ]; then
        print_err 'Not logged in as root to carry out this operation: Permission denied'
        return 1
    fi
    return 0
}

ADD_RIGHT_X() {
    # Add execution permission (+x) to a file or directory
    obj="$1"

    if [ ! -e "$obj" ]; then
        print_err "file/directory '$obj' does not exist"
        return 1
    fi

    if [ ! -x "$obj" ]; then
        print_info "*** chmod +x '$obj' ***"
        chmod +x "$obj" 2>/dev/null || {
            print_err "Failed to chmod +x '$obj'"
            return 1
        }
    fi

    return 0
}

ADD_RIGHT_W() {
    # Add write permission (+w) to a file or directory
    obj="$1"

    if [ ! -e "$obj" ]; then
        print_err "file/directory '$obj' does not exist"
        return 1
    fi

    if [ ! -w "$obj" ]; then
        print_info "*** chmod +w '$obj' ***"
        chmod +w "$obj" 2>/dev/null || {
            print_err "Failed to chmod +w '$obj'"
            return 1
        }
    fi

    return 0
}

ADD_RIGHT_R() {
    # Add read permission (+r) to a file or directory
    obj="$1"

    if [ ! -e "$obj" ]; then
        print_err "file/directory '$obj' does not exist"
        return 1
    fi

    if [ ! -r "$obj" ]; then
        print_info "*** chmod +r '$obj' ***"
        chmod +r "$obj" 2>/dev/null || {
            print_err "Failed to chmod +r '$obj'"
            return 1
        }
    fi

    return 0
}

ADD_DIR() {
    # Create a directory path with private user-only permissions (0700) on all created levels
    dir="$1"

    if [ ! -d "$dir" ]; then
        print_info "*** mkdir -m 0700 -p '$dir' ***"
        # Temporarily restrict umask so intermediate parent directories are also private
        if ! ( umask 0077 && mkdir -p "$dir" ) 2>/dev/null; then
            print_err "Failed to create directory '$dir'"
            return 1
        fi
    fi

    return 0
}

ADD_FILE() {
    # Create a file
    # File parameter must be an absolute path, otherwise created in current directory
    file="$1"
    dir=$(dirname "$file")

    if [ ! -d "$dir" ]; then
        print_err "Directory '$dir' does not exist"
        return 1
    fi

    if [ ! -f "$file" ]; then
        print_info "*** touch '$file' ***"
        touch "$file" || {
            print_err "Failed to create file '$file'"
            return 1
        }
    fi

    return 0
}

# write_proc <path> <value> <label>
# Single point of truth for every /proc write: checks the value is set,
# checks the file is writable, reports precisely on failure.
write_proc() {
    wp_path="$1"
    wp_value="$2"
    wp_label="$3"

    if [ -z "$wp_value" ]; then
        print_err "$wp_label variable is empty or not set"
        return 1
    fi

    if [ ! -w "$wp_path" ]; then
        print_err "Cannot write to $wp_path (file missing or permission denied)"
        return 1
    fi

    if ! printf '%s' "$wp_value" > "$wp_path" 2>/dev/null; then
        print_err "Failed to write to $wp_path"
        return 1
    fi

    return 0
}

# =======
# PARSING
# =======
# parses kernforgex main configuration file and loads it
load_config() {
    if ! parser "$CONF_FILE" ; then    
        print_err "Failed to parse configuration file '$CONF_FILE'"
        return 1
    fi
    return 0
}

#========
# ALIASES
#========

#TODO: alias removing and setting in a hidden file .aliases under ~/.kernforgex

#===================================
# PACKAGES INSTALLATION AND REMOVING
#===================================

MISSING_PACKAGES=""
INSTALLED_PACKAGES=""

IS_INSTALLED_PACKAGES() {
    # Check package statuses and populate global variables:
    # - INSTALLED_PACKAGES for removal ("r")
    # - MISSING_PACKAGES for installation ("i")

    packages_list="$1"
    to="$2" # "i" for installation, "r" for removal

    # NOTE: reset on every call, otherwise results from a previous call in
    # the same shell session (script sourced, or called more than once)
    # accumulate instead of being recomputed.
    MISSING_PACKAGES=""
    INSTALLED_PACKAGES=""

    # Iterate over space-separated package names
    for pkg in $packages_list; do
        if ! dpkg-query -W -f='${Status}' "$pkg" 2>/dev/null | grep -q "install ok installed"; then
            printf "%b '%s'\n" "$NOT" "$pkg"
            if [ "$to" = "i" ]; then
                MISSING_PACKAGES="$MISSING_PACKAGES $pkg"
            fi
        else
            printf "%b '%s'\n" "$OK" "$pkg"
            if [ "$to" = "r" ]; then
                INSTALLED_PACKAGES="$INSTALLED_PACKAGES $pkg"
            fi
        fi
    done

    # Check non-empty strings instead of array length
    if [ -n "$MISSING_PACKAGES" ] && [ "$to" = "i" ]; then
        return 1
    fi

    if [ -n "$INSTALLED_PACKAGES" ] && [ "$to" = "r" ]; then
        return 1
    fi

    return 0
}

INSTALL_PACKAGES() {
    # Install all missing packages listed in global $MISSING_PACKAGES

    FAILED_PACKAGES=""

    return
    if [ -n "$MISSING_PACKAGES" ]; then
        IS_LOGIN_ROOT || return 1

        print_info '--- installing missing packages ---'

        apt-get update >/dev/null 2>&1

        for pkg in $MISSING_PACKAGES; do
            printf "[ %s ]──╼ " "$pkg"
            if ! DEBIAN_FRONTEND=noninteractive apt-get install -y "$pkg" >/dev/null 2>&1; then
                printf "%b❌%b\n" "$RED" "$NC"
                FAILED_PACKAGES="$FAILED_PACKAGES $pkg"
            else
                printf "%b✔%b\n" "$GREEN" "$NC"
            fi
        done
    else
        return 0
    fi

    if [ -n "$FAILED_PACKAGES" ]; then
        print_err "--| following packages installation failed:"

        for pkg in $FAILED_PACKAGES; do
            printf "%s  " "$pkg"
        done
        printf "\n"
        return 1
    else
        print_success "---| successfully finished all packages installation"
    fi

    return 0
}

REMOVE_PACKAGES() {
    # Uninstall packages listed in global $INSTALLED_PACKAGES

    FAILED_PACKAGES=""

    if [ -n "$INSTALLED_PACKAGES" ]; then
        IS_LOGIN_ROOT || return 1

        print_info '--- removing packages ---'

        for pkg in $INSTALLED_PACKAGES; do
            printf "[ %s ]──╼ " "$pkg"
            if ! (DEBIAN_FRONTEND=noninteractive apt-get remove -y "$pkg" >/dev/null 2>&1 && apt-get purge -y "$pkg" >/dev/null 2>&1); then
                printf "%b❌%b\n" "$RED" "$NC"
                FAILED_PACKAGES="$FAILED_PACKAGES $pkg"
            else
                printf "%b✔%b\n" "$GREEN" "$NC"
            fi
        done
    else
        return 0
    fi

    apt-get clean

    if [ -n "$FAILED_PACKAGES" ]; then
        print_err "---| following packages removal failed:"

        for pkg in $FAILED_PACKAGES; do
            printf "%s  " "$pkg"
        done
        printf "\n"
        return 1
    else
        print_success "---| successfully finished packages removal"
    fi

    return 0
}