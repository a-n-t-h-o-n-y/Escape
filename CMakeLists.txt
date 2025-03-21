cmake_minimum_required(VERSION 3.16)
project(Escape LANGUAGES CXX)

# UTF Support
find_package(ICU REQUIRED COMPONENTS dt uc i18n)
if(NOT ICU_FOUND)
    message(FATAL_ERROR "ICU not found")
endif()

# Utility Library Dependency
include(FetchContent)
FetchContent_Declare(
    zzz
    GIT_REPOSITORY https://github.com/a-n-t-h-o-n-y/zzz.git
    GIT_TAG 3df70f6fc683b2b03dc732ab63346d5c59891870
)
FetchContent_MakeAvailable(zzz)

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
    include/esc/detail/is_urxvt.hpp
    include/esc/detail/mask.hpp
    include/esc/detail/signals.hpp
    include/esc/detail/transcode.hpp
    include/esc/detail/tty_file.hpp

    src/io.cpp
    src/terminfo.cpp
    src/terminal.cpp
    src/sequence.cpp
    src/detail/is_urxvt.cpp
    src/detail/transcode.cpp
    src/detail/console_file.cpp
    src/detail/signals.cpp
)

target_include_directories(escape
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(escape
    PUBLIC
        zzz
        ICU::dt
        ICU::uc
        ICU::i18n
)

target_compile_features(escape
    PUBLIC
        cxx_std_20
)

add_subdirectory(tools)
add_subdirectory(examples)
add_subdirectory(tests)