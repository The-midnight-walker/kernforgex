# SPDX-License-Identifier: GPL-2.0
#
# vim: set ts=8 sw=8 noet tw=80 cc=80 fo+=t :

cmake_minimum_required(VERSION 3.10)
project(kfx LANGUAGES C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

file(GLOB_RECURSE FORMAT_FILES
    CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/*.c"
    "${CMAKE_SOURCE_DIR}/*.h"
)

list(FILTER FORMAT_FILES EXCLUDE REGEX "^${CMAKE_SOURCE_DIR}/build/")

#------| formating code text with clang-format
find_program(CLANG_FORMAT_BIN NAMES clang-format clang-format-16 clang-format-15)

if(CLANG_FORMAT_BIN)
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_BIN} -i -style=file ${FORMAT_FILES}
        COMMENT "Formating code with clang-format..."
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
else()
    message(WARNING "clang-format package not found. Target 'format' unavalaible.")
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

enable_testing()
add_subdirectory(tests)

#TODO: install
#install(TARGETS program DESTINATION bin)