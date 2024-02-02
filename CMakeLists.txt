cmake_minimum_required(VERSION 3.16)
project(escape LANGUAGES CXX)

# Escape Library
add_library(escape STATIC
    include/esc/glyph.hpp
    include/esc/detail/any_of.hpp
    include/esc/detail/transcode.hpp

    src/io.cpp
    src/terminfo.cpp
    src/terminal.cpp
    src/trait.cpp
    src/sequence.cpp
    src/point.cpp
    src/mouse.cpp
    src/detail/display.cpp
    src/detail/is_urxvt.cpp
    src/detail/print.cpp
    src/detail/transcode.cpp
    src/detail/traits_to_int_sequence.cpp
    src/detail/console_file.cpp
    src/detail/signals.cpp
)

# UTF Support
find_package(ICU REQUIRED COMPONENTS dt uc i18n)
if(ICU_FOUND)
    target_link_libraries(escape ICU::dt ICU::uc ICU::i18n)
endif()

target_include_directories(escape
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(escape
    PUBLIC
        cxx_std_20
)

target_compile_options(escape
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

# TermCaps Executable
add_executable(termcaps EXCLUDE_FROM_ALL
    tools/termcaps.cpp
)

target_link_libraries(termcaps
    PRIVATE
        escape
)

target_compile_options(termcaps
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

if (NOT TARGET Catch2)
    add_subdirectory(external/Catch2)
endif()

add_subdirectory(tests)