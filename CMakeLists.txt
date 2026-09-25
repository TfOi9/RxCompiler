cmake_minimum_required(VERSION 3.16)
project(rx_compiler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(antlr4-runtime CONFIG REQUIRED)

file(GLOB_RECURSE RX_COMPILER_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_executable(rx_compiler ${RX_COMPILER_SOURCES})
target_include_directories(rx_compiler PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(rx_compiler PRIVATE antlr4_shared)
