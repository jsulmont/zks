project(zks C CXX)
cmake_minimum_required(VERSION 3.8)

set(CMAKE_MODULE_PATH
    ${CMAKE_MODULE_PATH}
    ${Aptol_SOURCE_DIR}/cmake
    ${CMAKE_SOURCE_DIR}/cmake/externals/sanitizers-cmake/cmake
)

find_package(Sanitizers)

set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_LIBS ON)

# format is header only but it allows to find boost include dirs.
find_package(Boost
             COMPONENTS system
             REQUIRED)

include_directories(SYSTEM ${Boost_INCLUDE_DIR})
link_directories(${Boost_LIBRARY_DIR})

find_program(CCACHE_FOUND ccache)
if (CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif()

set(CMAKE_CXX_STANDARD 17 CACHE STRING "C++ standard for all targets.")
set(CMAKE_CXX_STANDARD_REQUIRED true)

if (${CMAKE_CXX_COMPILER_ID} MATCHES GNU OR ${CMAKE_CXX_COMPILER_ID} MATCHES Clang)
    add_compile_options(-W -Wextra -Wall -Wno-unused-parameter)
    add_compile_options("-Wno-unused-local-typedefs")
    add_compile_options("-Wno-unused-function")
endif()

add_executable(
    zks
        avalanche.cpp
        avalanche.hpp
        cxxopts.hpp
        parameters.hpp
        main.cpp
    )
add_sanitizers(zks)


find_program(CLANG_FORMAT
        NAMES
        clang-format
        clang-format-9.0
        clang-format-8.1
        clang-format-8.0
        clang-format-7.0
        clang-format-6.2
        clang-format-6.0
        clang-format-5.2
        clang-format-5.0
        clang-format-4.0
        clang-format-3.9
        clang-format-3.8
        )


if (CLANG_FORMAT)
    file(GLOB_RECURSE ALL_SOURCE_FILES
            **.cpp
            **.h
            **.hpp
            **.c
            **.cc
    )
    add_custom_target(
            format
            COMMAND
            ${CLANG_FORMAT}
            -style=google
            -i
            ${ALL_SOURCE_FILES})
endif()
