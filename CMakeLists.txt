cmake_minimum_required(VERSION 3.12)
project(escape LANGUAGES CXX)

add_library(
    escape STATIC
        src/io.cpp
)
target_include_directories(escape PUBLIC include/)
target_compile_features(escape PUBLIC cxx_std_17)
target_compile_options(escape PUBLIC -Wall -Wextra)

add_executable(escape-test test/escape.test.cpp)
target_link_libraries(escape-test PRIVATE escape)
