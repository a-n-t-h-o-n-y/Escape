add_executable(escape.benchmarks main.cpp)
target_link_libraries(escape.benchmarks PRIVATE Escape::escape)
target_compile_options(escape.benchmarks PRIVATE ${ESCAPE_WARNINGS})
set_target_properties(escape.benchmarks PROPERTIES INTERPROCEDURAL_OPTIMIZATION OFF)

if(ESCAPE_BUILD_TESTS OR ESCAPE_BUILD_CONTRACT_TESTS)
    add_test(NAME escape.benchmarks.smoke
        COMMAND escape.benchmarks --milliseconds 1 --samples 3)
    set_tests_properties(escape.benchmarks.smoke PROPERTIES TIMEOUT 30)
endif()
