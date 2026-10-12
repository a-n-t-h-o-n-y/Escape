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
        add_test(NAME escape.docs.examples
            COMMAND "${Python3_EXECUTABLE}"
                "${PROJECT_SOURCE_DIR}/tests/contract/verify_docs.py"
                --source "${PROJECT_SOURCE_DIR}"
                --output "$<TARGET_FILE:escape.tutorial.output>"
                --layout "$<TARGET_FILE:escape.tutorial.layout>"
                --picker "$<TARGET_FILE:escape.tutorial.picker>")
        set_tests_properties(escape.docs.examples PROPERTIES TIMEOUT 30 RUN_SERIAL TRUE)
        add_test(NAME escape.contract.LUMEN.interactive
            COMMAND "${Python3_EXECUTABLE}"
                "${PROJECT_SOURCE_DIR}/tests/contract/smoke_lumen.py"
                --lumen "$<TARGET_FILE:escape.examples.lumen>")
        set_tests_properties(escape.contract.LUMEN.interactive
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

add_executable(escape.examples.lumen lumen.cpp)
target_compile_options(escape.examples.lumen PRIVATE ${ESCAPE_WARNINGS})
target_link_libraries(escape.examples.lumen PRIVATE escape)

if(ESCAPE_BUILD_TESTS OR ESCAPE_BUILD_CONTRACT_TESTS)
    add_executable(escape.tests.lumen-canvas
        ../tests/contract/main.cpp ../tests/contract/lumen_canvas.test.cpp)
    target_link_libraries(escape.tests.lumen-canvas PRIVATE escape)
    target_compile_options(escape.tests.lumen-canvas PRIVATE ${ESCAPE_WARNINGS})
    add_test(NAME escape.contract.LUMEN.canvas COMMAND escape.tests.lumen-canvas)
    set_tests_properties(escape.contract.LUMEN.canvas PROPERTIES TIMEOUT 30)
endif()

foreach(example output layout picker)
    add_executable(escape.tutorial.${example} tutorials/${example}.cpp)
    target_compile_options(escape.tutorial.${example} PRIVATE ${ESCAPE_WARNINGS})
    target_link_libraries(escape.tutorial.${example} PRIVATE escape)
endforeach()
