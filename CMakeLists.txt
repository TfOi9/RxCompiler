cmake_minimum_required(VERSION 3.16)
project(rx_compiler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(antlr4-runtime 4.13.2 EXACT CONFIG QUIET)

if(antlr4-runtime_FOUND)
    set(RX_ANTLR4_TARGET antlr4_shared)
    message(STATUS "ANTLR C++ runtime: 4.13.2 (system)")
else()
    # Build the matching vendored runtime without archive extraction or downloads.
    set(ANTLR_BUILD_CPP_TESTS OFF CACHE BOOL "Build ANTLR tests" FORCE)
    set(ANTLR_BUILD_SHARED OFF CACHE BOOL "Build shared ANTLR runtime" FORCE)
    set(ANTLR_BUILD_STATIC ON CACHE BOOL "Build static ANTLR runtime" FORCE)
    set(ANTLR4_INSTALL OFF CACHE BOOL "Install ANTLR runtime" FORCE)
    set(WITH_DEMO OFF CACHE BOOL "Build ANTLR demo" FORCE)
    add_subdirectory(vendor/antlr4/runtime-4.13.2)
    set(ANTLR4_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/antlr4/runtime-4.13.2/runtime/src")
    set(RX_ANTLR4_TARGET antlr4_static)
    message(STATUS "ANTLR C++ runtime: 4.13.2 (bundled source)")
endif()

file(GLOB_RECURSE RX_COMPILER_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_executable(rx_compiler ${RX_COMPILER_SOURCES})
target_include_directories(rx_compiler PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_include_directories(rx_compiler SYSTEM PRIVATE "${ANTLR4_INCLUDE_DIR}")
target_link_libraries(rx_compiler PRIVATE ${RX_ANTLR4_TARGET})
