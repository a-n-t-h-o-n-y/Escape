cmake_minimum_required(VERSION 3.8)
project(escape LANGUAGES CXX)

add_library(
    escape STATIC
        src/io.cpp
        src/terminfo.cpp
        src/terminal.cpp
)

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

target_include_directories(escape PUBLIC include/)
target_compile_features(escape PUBLIC cxx_std_17)
target_compile_options(escape PUBLIC -Wall -Wextra)

add_executable(termcaps EXCLUDE_FROM_ALL tools/termcaps.cpp)
target_link_libraries(termcaps PRIVATE escape)
