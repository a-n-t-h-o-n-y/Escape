# TermCaps Executable
add_executable(termcaps
    termcaps.cpp
)

target_link_libraries(termcaps
    PRIVATE
        escape
)

target_compile_options(termcaps
    PRIVATE
        ${ESCAPE_WARNINGS}
)
