cmake_minimum_required(VERSION 3.16)
project(escape LANGUAGES CXX)

# Escape Library
add_library(escape STATIC
    include/esc/area.hpp
    include/esc/brush.hpp
    include/esc/color.hpp
    include/esc/esc.hpp
    include/esc/event.hpp
    include/esc/glyph.hpp
    include/esc/io.hpp
    include/esc/key.hpp
    include/esc/mouse.hpp
    include/esc/point.hpp
    include/esc/sequence.hpp
    include/esc/terminal.hpp
    include/esc/terminfo.hpp
    include/esc/trait.hpp
    include/esc/detail/any_of.hpp
    include/esc/detail/console_file.hpp
    include/esc/detail/debug.hpp
    include/esc/detail/is_urxvt.hpp
    include/esc/detail/mask.hpp
    include/esc/detail/signals.hpp
    include/esc/detail/transcode.hpp

    src/io.cpp
    src/terminfo.cpp
    src/terminal.cpp
    src/trait.cpp
    src/sequence.cpp
    src/mouse.cpp
    src/detail/debug.cpp
    src/detail/is_urxvt.cpp
    src/detail/transcode.cpp
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

add_subdirectory(examples)
add_subdirectory(tests)