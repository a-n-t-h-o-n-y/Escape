# Pong Example
add_executable(escape.examples.pong
    pong.cpp
)

target_compile_options(
    escape.examples.pong
    PRIVATE
        ${ESCAPE_WARNINGS}
)

target_link_libraries(
    escape.examples.pong
    PRIVATE
        escape
)
if(UNIX AND (ESCAPE_BUILD_TESTS OR ESCAPE_BUILD_CONTRACT_TESTS))
    find_package(Python3 QUIET COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME escape.contract.EXAMPLES.smoke
            COMMAND "${Python3_EXECUTABLE}"
                "${PROJECT_SOURCE_DIR}/tests/contract/smoke_examples.py"
                --termcaps "$<TARGET_FILE:termcaps>"
                --pong "$<TARGET_FILE:escape.examples.pong>"
                --event-loop "$<TARGET_FILE:escape.examples.event-loop>")
        set_tests_properties(escape.contract.EXAMPLES.smoke
            PROPERTIES TIMEOUT 30 RUN_SERIAL TRUE)
        add_test(NAME escape.contract.TERMCAPS.interactive
            COMMAND "${Python3_EXECUTABLE}"
                "${PROJECT_SOURCE_DIR}/tests/contract/smoke_termcaps.py"
                --termcaps "$<TARGET_FILE:termcaps>")
        set_tests_properties(escape.contract.TERMCAPS.interactive
            PROPERTIES TIMEOUT 30 RUN_SERIAL TRUE)
    endif()
endif()

add_executable(escape.examples.event-loop event-loop.cpp)
target_compile_options(escape.examples.event-loop PRIVATE ${ESCAPE_WARNINGS})
target_link_libraries(escape.examples.event-loop PRIVATE escape)
