# SPDX-License-Identifier: GPL-2.0
#
# vim: set ts=8 sw=8 noet tw=80 cc=80 fo+=t :

MISSING_PACKAGES=()
INSTALLED_PACKAGES=()
declare -A MISSING_TOOLS

RED="\033[1;31m"
GREEN="\033[1;32m"
YELLOW="\033[1;33m"
NC="\033[0m"

OK="${GREEN}[ ok ]${NC}"
NOT="${RED}[ X  ]${NC}"

print_err() {
    local msg="$1"
    echo -e "${RED}${msg}${NC}"
}

print_success() {
    local msg="$1"
    echo -e "${GREEN}${msg}${NC}"
}

print_info() {
    local msg="$1"
    echo -e "${YELLOW}${msg}${NC}"
}

IS_LOGIN_ROOT() {
    # verify this script is running with root privilege

    if [ "$EUID" -ne 0 ]; then
        print_err 'Not login as root to carry out this operation : Permission denied'
        exit -1
    fi
    return
}

add_maps() {
    # concatenate two maps

    local -n dest="$1"
    local -n src=$2

    for key in "${!src[@]}"; do
        dest["$key"]="${src[$key]}"
    done
}

IS_INSTALLED_PACKAGES() {
    # check tools which are already install and adding
    # | install packages in INSTALLED_PACKAGES  for removing
    # | missing packages in MISSSING_PACKAGES for installation

    local -n packages="$1"
    local to="$2" #for installation or removing

    IS_LOGIN_ROOT

    for pkg in "${packages[@]}"; do
        if ! dpkg-query -W -f='${Status}' "$pkg" 2>/dev/null | grep -q "install ok installed"; then
            echo -e "${NOT} '$pkg'"
            if [ "$to" == "i" ]; then
                MISSING_PACKAGES+=("$pkg") #installation purpose
            fi
        else
            echo -e "${OK} '$pkg'"
            if [ "$to" == "u" ]; then
                INSTALLED_PACKAGES+=("$pkg") # removing purpose
            fi
        fi
    done

    if [[ "${#MISSING_PACKAGES[@]}" -gt 0 && "$to" == "i" ]]; then
        return 1
    fi

    if [[ "${#INSTALLED_PACKAGES[@]}" -gt 0 && "$to" == "u" ]]; then
        return 1
    fi

    return 0
}

IS_DOWNLOAD_TOOLS() {
    # check tools which are already download under the $HOME/lkm/tools directory
    # if not ,these latter are adding in the MISSING_TOOLS["$tool"]="${tools[$tool]}" array

    local -n tools="$1"

    for tool in "${!tools[@]}"; do
        if [ ! -f "$tool" ]; then
            echo -e "${NOT} '$tool'"
            MISSING_TOOLS["$tool"]="${tools[$tool]}"
        else
            echo -e "${OK} '$tool'"
        fi
    done
}

INSTALL_PACKAGES() {
    # install all missing packages

    local FAILLED_PACKAGES=()

    if [ ${#MISSING_PACKAGES[@]} -gt 0 ]; then
        IS_LOGIN_ROOT

        print_info '--- installing missing packages ---'

        apt-get update >/dev/null 2>&1

        for pkg in "${MISSING_PACKAGES[@]}"; do
            echo -n "[ $pkg ]──╼ "
            if ! DEBIAN_FRONTEND=noninteractive apt-get install -y "$pkg" >/dev/null 2>&1; then
                echo -en "${RED}❌${NC}"
                echo
                FAILLED_PACKAGES+=("$pkg")
            else
                echo -en "${GREEN}✔${NC}"
                echo
            fi
        done
    else
        return
    fi

    if [ ${#FAILLED_PACKAGES[@]} -gt 0 ]; then
        print_err "--| following packages installation failled"

        for pkg in "${FAILLED_PACKAGES[@]}"; do
            echo -n ""$pkg"  "
        done
        echo
    else
        print_success "---| successfull finish all packages installation"
    fi

    return 0
}

REMOVE_PACKAGES() {
    # uninstall packages

    local FAILLED_PACKAGES=()

    if [ ${#INSTALLED_PACKAGES[@]} -gt 0 ]; then
        IS_LOGIN_ROOT

        print_info '--- removing packages ---'

        for pkg in "${INSTALLED_PACKAGES[@]}"; do
            echo -n "[ $pkg ]──╼ "
            if ! (DEBIAN_FRONTEND=noninteractive apt-get remove -y "$pkg" >/dev/null 2>&1 && apt-get purge -y "$pkg" >/dev/null 2>&1); then
                echo -en "${RED}❌${NC}"
                echo
                FAILLED_PACKAGES+=("$pkg")
            else
                echo -en "${GREEN}✔${NC}"
                echo
            fi
        done
    else
        return
    fi

    apt-get clean

    if [ ${#FAILLED_PACKAGES[@]} -gt 0 ]; then
        print_err "---| following packages removing failled"

        for pkg in "${FAILLED_PACKAGES[@]}"; do
            echo ""$pkg"  "
        done
        echo
    else
        print_success "---| successfull finish packages removing"
    fi

    return

}

ERASE_ALIASES() {
    # delete fgx alias in ~/.bashrc file

    local bashrc=~/.bashrc

    sed -i '/#   ===========================/,/#  ===========================/d' "$bashrc"
}

GET_ALIASES() {
    # collecting all aliases to set

    local -n input_map="$1"

    for alias in "${!input_map[@]}"; do
        ALIASES["$alias"]="${input_map[$alias]}"
    done
}

SET_ALIASES() {
    # set aliases in the ~/.bashrc file

    local bashrc=~/.bashrc

    if [ "${#ALIASES[@]}" -eq 0 ]; then
        return
    fi

    echo "overwrite previous aliases.."
    ERASE_ALIASES

    print_info "--- setting new aliases ---"

    cat <<'EOF' >>"$bashrc"
#   ===========================
#    [ KERNFORGEX - ALIASES ]
#   ===========================
EOF

    for alias in "${!ALIASES[@]}"; do
        local value="${ALIASES[$alias]}"
        echo "alias $alias='$value'" | tee -a "$bashrc"
    done

    echo "#  ===========================" >>"$bashrc"
}

DOWNLOAD() {
    # dowload files which missing
    # @MISSING_TOOLS - dictionnary array : MISSING_TOOLS['tool_path'] = 'download_url'

    IS_LOGIN_ROOT

    if [ ${#MISSING_TOOLS[@]} -eq 0 ]; then
        return 0
    fi

    print_info '--- downloading missing tools ---'

    for tool_path in "${!MISSING_TOOLS[@]}"; do
        local url="${MISSING_TOOLS[$tool_path]}"
        local target_dir
        target_dir=$(dirname "$tool_path")

        mkdir -p "$target_dir" 2>/dev/null

        if wget -O "$tool_path" "$url" >/dev/null 2>&1; then
            print_success "Successfully downloaded '$tool_path'"
        else
            print_err "Failed to download '$tool_path' from '$url'"
        fi
    done
}

ADD_RIGHT_X() {
    # add the executable right 'x' to a objetc : a file or a directory

    local obj="$1"

    if [ ! -f "$obj" ]; then
        pr_err "file/directory '$obj' does'nt exists"
        return 1
    fi

    if [ ! -x "$obj" ]; then
        print_info "*** chmod +x '$obj' ***"
        chmod +x "$obj" 2>/dev/null || true
    fi

    return 0
}

ADD_DIR() {
    # create a directory with exclusive private rights to the user

    local dir="$1"

    if [ ! -d "$dir" ]; then
        print_info "*** mkdir -m 0700 -p '$dir'***"
        mkdir 0700 -p "$dir" 2>/dev/null || true
    fi

    return 0
}

ADD_FILE() {
    # create a file
    # @file must be a absolute path otherwise file will be create in the current  directory

    local file="$1"
    local dir
    dir=$(dirname "$file")

    if [ ! -d "$dir" ]; then
        print_err "Directory '$dir' does not exist"
        return 1
    fi

    if [ ! -f "$file" ]; then
        print_info "*** touch '$file' ***"
        touch "$file"
    fi

    return 0
}
