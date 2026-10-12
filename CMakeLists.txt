cmake_minimum_required(VERSION 3.16)
project(Escape VERSION 0.3.0 LANGUAGES CXX)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(ESCAPE_TOP_LEVEL ON)
else()
    set(ESCAPE_TOP_LEVEL OFF)
endif()
option(ESCAPE_BUILD_TESTS "Build behavior tests" ${ESCAPE_TOP_LEVEL})
option(ESCAPE_BUILD_EXAMPLES "Build examples and tools" ${ESCAPE_TOP_LEVEL})
option(ESCAPE_BUILD_BENCHMARKS "Build standalone performance benchmarks" OFF)
option(ESCAPE_BUILD_CONTRACT_TESTS "Build behavior tests (compatibility option)" OFF)

if(MSVC)
    set(ESCAPE_WARNINGS /W4)
else()
    set(ESCAPE_WARNINGS -Wall -Wextra -Wpedantic)
endif()

add_library(escape
    src/text.cpp src/layout.cpp src/glyph.cpp src/sequence.cpp src/extensions.cpp src/capabilities.cpp
    src/parser.cpp src/query.cpp src/keyboard.cpp src/io.cpp src/terminal.cpp src/detail/backend_common.cpp)
if(WIN32)
    target_sources(escape PRIVATE src/detail/backend_windows.cpp)
    target_compile_definitions(escape PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
else()
    target_sources(escape PRIVATE src/detail/backend.cpp)
endif()
if(MSVC)
    target_compile_options(escape PUBLIC /utf-8)
endif()
set_target_properties(escape PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
add_library(Escape::escape ALIAS escape)
target_compile_features(escape PUBLIC cxx_std_20)
target_include_directories(escape PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
find_package(Threads REQUIRED)
target_link_libraries(escape PRIVATE Threads::Threads)

if(ESCAPE_BUILD_TESTS OR ESCAPE_BUILD_CONTRACT_TESTS)
    enable_testing()
    add_subdirectory(tests/contract)
endif()
if(ESCAPE_BUILD_EXAMPLES)
    add_subdirectory(tools)
    add_subdirectory(examples)
endif()
if(ESCAPE_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

install(TARGETS escape EXPORT EscapeTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/esc DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT EscapeTargets NAMESPACE Escape:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Escape)
configure_package_config_file(cmake/EscapeConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/EscapeConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Escape)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/EscapeConfigVersion.cmake
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMinorVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/EscapeConfig.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/EscapeConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Escape)

install(FILES LICENSE.unicode DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/Escape)
