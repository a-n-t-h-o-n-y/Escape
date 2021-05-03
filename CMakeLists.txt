cmake_minimum_required(VERSION 3.9)
project(escape LANGUAGES CXX)

# Escape Library
add_library(
    escape STATIC
        src/io.cpp
        src/terminfo.cpp
        src/terminal.cpp
)

target_include_directories(escape PUBLIC include/)
target_compile_features(escape PUBLIC cxx_std_17)
target_compile_options(escape PUBLIC -Wall -Wextra)

# Link-Time Optimization
include(CheckIPOSupported)
check_ipo_supported(RESULT has_lto OUTPUT error)
if(has_lto)
    set_property(TARGET escape PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    message("LTO Enabled for EscapeLib")
else()
    message(WARNING "LTO is not supported: ${error}")
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
add_executable(termcaps EXCLUDE_FROM_ALL tools/termcaps.cpp)
target_link_libraries(termcaps PRIVATE escape)
