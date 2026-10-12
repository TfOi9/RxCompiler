cmake_minimum_required(VERSION 3.16)
project(rx_compiler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(antlr4-runtime 4.13.2 EXACT CONFIG QUIET)

if(antlr4-runtime_FOUND)
    set(RX_ANTLR4_TARGET antlr4_shared)
    message(STATUS "ANTLR C++ runtime: 4.13.2 (system)")
else()
    # Keep OJ builds independent of the installed runtime version and network.
    include(FetchContent)
    if(POLICY CMP0135)
        cmake_policy(SET CMP0135 NEW)
    endif()
    set(ANTLR_BUILD_CPP_TESTS OFF CACHE BOOL "Build ANTLR tests" FORCE)
    set(ANTLR_BUILD_SHARED OFF CACHE BOOL "Build shared ANTLR runtime" FORCE)
    set(ANTLR_BUILD_STATIC ON CACHE BOOL "Build static ANTLR runtime" FORCE)
    set(ANTLR4_INSTALL OFF CACHE BOOL "Install ANTLR runtime" FORCE)
    set(WITH_DEMO OFF CACHE BOOL "Build ANTLR demo" FORCE)
    FetchContent_Declare(rx_antlr4_runtime
        URL "${CMAKE_CURRENT_SOURCE_DIR}/vendor/antlr4/antlr4-cpp-runtime-4.13.2-source.zip"
        URL_HASH SHA256=0ed13668906e86dbc0dcddf30fdee68c10203dea4e83852b4edb810821bee3c4
    )
    FetchContent_MakeAvailable(rx_antlr4_runtime)
    set(ANTLR4_INCLUDE_DIR "${rx_antlr4_runtime_SOURCE_DIR}/runtime/src")
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
