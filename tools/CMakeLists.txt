# TermCaps Executable
add_executable(termcaps EXCLUDE_FROM_ALL
    termcaps.cpp
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
