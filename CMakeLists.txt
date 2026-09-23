# SPDX-License-Identifier: GPL-2.0
#
# vim: set ts=8 sw=8 noet tw=80 cc=80 fo+=t :

cmake_minimum_required(VERSION 3.10)
project(kfgx LANGUAGES C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

file(GLOB_RECURSE FORMAT_FILES
    CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/*.c"
    "${CMAKE_SOURCE_DIR}/*.h"
)

file(GLOB_RECURSE SHELL_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/scripts/*.sh"
)

list(FILTER FORMAT_FILES EXCLUDE REGEX "^${CMAKE_SOURCE_DIR}/build/")

# =======
# TARGETS
# =======

# Target formats and styles all (shell and c code)
add_custom_target(format
    COMMENT "--- Running full C and Shell formatting ---"
)

#------| formating code text with clang-format
find_program(CLANG_FORMAT_BIN clang-format)
if(CLANG_FORMAT_BIN)
    add_custom_target(format-c
        COMMAND ${CLANG_FORMAT_BIN} -i -style=file ${C_FILES}
        COMMENT "[clang-format] Formatting C source files..."
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_dependencies(format format-c)
else()
    message(WARNING "clang-format not found. C formatting skipped.")
endif()

# --- Targeting shfmt (Shell Formatter) ---
find_program(SHFMT_BIN shfmt)
if(SHFMT_BIN)
    add_custom_target(format-shell
        COMMAND ${SHFMT_BIN} -w ${SHELL_FILES}
        COMMENT "[shfmt] Formatting shell scripts (reads .editorconfig)..."
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_dependencies(format format-shell)
else()
    message(WARNING "shfmt not found. Shell formatting skipped.")
endif()

# --- Targeting shellcheck (Shell Linter) ---
find_program(SHELLCHECK_BIN shellcheck)
if(SHELLCHECK_BIN)
    add_custom_target(lint-shell
        COMMAND ${SHELLCHECK_BIN} ${SHELL_FILES}
        COMMENT "[shellcheck] Linting shell scripts (reads .shellcheckrc)..."
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_dependencies(format lint-shell)
else()
    message(WARNING "shellcheck not found. Shell linting skipped.")
endif()

#------| documentation with Doxygen
find_package(Doxygen)

if(DOXYGEN_FOUND)
    add_custom_target(doc
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "genrating documentation with Doxygen..."
        VERBATIM
    )
endif()

add_subdirectory(src)

# =======
# TESTING
# =======

enable_testing()
add_subdirectory(tests)

#TODO: install
#install(TARGETS program DESTINATION bin)