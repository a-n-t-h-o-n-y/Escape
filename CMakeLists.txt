cmake_minimum_required(VERSION 3.12)

add_library(escape INTERFACE)
target_include_directories(escape INTERFACE include/)
target_compile_features(escape INTERFACE cxx_std_17)
target_compile_options(escape INTERFACE -Wall -Wextra)

add_executable(escape-test test/escape.test.cpp)
