cmake_minimum_required(VERSION 3.16)
project(escape LANGUAGES CXX)

# Escape Library
add_library(escape STATIC
    src/io.cpp
    src/terminfo.cpp
    src/terminal.cpp
    src/trait.cpp
    src/sequence.cpp
    src/point.cpp
    src/mouse.cpp
    src/detail/display.cpp
    src/detail/is_urxvt.cpp
    src/detail/mb_to_u32.cpp
    src/detail/print.cpp
    src/detail/traits_to_int_sequence.cpp
    src/detail/u32_to_mb.cpp
    src/detail/console_file.cpp
    src/detail/signals.cpp
)

target_include_directories(escape
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(escape
    PUBLIC
        cxx_std_17
)

target_compile_options(escape
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

option(LTO "Turn Link Time Optimization ON/OFF" ON)

# Link-Time Optimization
include(CheckIPOSupported)
check_ipo_supported(RESULT has_lto OUTPUT error)
if(has_lto AND LTO)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    message("LTO Enabled Globally")
elseif(NOT has_lto)
    message(WARNING "LTO is not supported: ${error}")
else()
    message("LTO Not Enabled")
endif()

include(GNUInstallDirs)
install(TARGETS escape
        ARCHIVE
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
        CONFIGURATIONS Release
)
install(
    DIRECTORY
        ${PROJECT_SOURCE_DIR}/include/esc
    DESTINATION
        ${CMAKE_INSTALL_INCLUDEDIR}
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
