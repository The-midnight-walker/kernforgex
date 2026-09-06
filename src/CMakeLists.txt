# SPDX-License-Identifier: GPL-2.0
#
# vim: set ts=8 sw=8 noet tw=80 cc=80 fo+=t :

file(GLOB_RECURSE SOURCE_FILES
    "${CMAKE_SOURCE_DIR}/include/*.c"
    "${CMAKE_SOURCE_DIR}/include/${PROJECT_NAME}/*.c"
    "${CMAKE_SOURCE_DIR}/src/*.c"
    "${CMAKE_SOURCE_DIR}/${PROJECT_NAME}/*.c"
    "${CMAKE_SOURCE_DIR}/${PROJECT_NAME}/*.c"
)

# executable name : kfx
add_executable(kfx ${SOURCE_FILES})

target_compile_features(${PROJECT_NAME} PRIVATE
    c_std_11
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    DEBUG
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    "${CMAKE_SOURCE_DIR}/src/${PROJECT_NAME}"
)

target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall
    -Wextra
    -Wshadow
    -Werror
    -Wconversion
    -Wformat=2
    -fmacro-prefix-map=${CMAKE_SOURCE_DIR}=.
)