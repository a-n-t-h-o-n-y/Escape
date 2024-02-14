add_executable(escape.tests.unit EXCLUDE_FROM_ALL
    glyph.test.cpp
)

target_compile_options(
    escape.tests.unit
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

target_link_libraries(
    escape.tests.unit
    PRIVATE
        escape
        Catch2::Catch2WithMain
)