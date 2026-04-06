cmake_minimum_required(VERSION 3.24)

project(rtb_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)

find_package(Protobuf REQUIRED)

add_library(rtb_project_warnings INTERFACE)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(
        rtb_project_warnings
        INTERFACE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()

add_subdirectory(proto)
add_subdirectory(src)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

add_subdirectory(bench)
