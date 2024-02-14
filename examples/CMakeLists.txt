add_executable(escape.examples.pong EXCLUDE_FROM_ALL
    pong.cpp
)

target_compile_options(
    escape.examples.pong
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

target_link_libraries(
    escape.examples.pong
    PRIVATE
        escape
)