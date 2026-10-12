add_executable(escape.tests.contract.capabilities EXCLUDE_FROM_ALL
    main.cpp capabilities.test.cpp capability-allocation.test.cpp support/allocations.cpp)
target_link_libraries(escape.tests.contract.capabilities PRIVATE escape)
target_compile_features(escape.tests.contract.capabilities PRIVATE cxx_std_20)
target_compile_options(escape.tests.contract.capabilities PRIVATE -Wall -Wextra -Wpedantic)
add_executable(escape.tests.contract.encoding EXCLUDE_FROM_ALL
    main.cpp sequence.test.cpp values.test.cpp encoding-allocation.test.cpp support/allocations.cpp)
target_link_libraries(escape.tests.contract.encoding PRIVATE escape)
add_executable(escape.tests.contract.text EXCLUDE_FROM_ALL
    main.cpp text.test.cpp text-allocation.test.cpp support/allocations.cpp)
target_link_libraries(escape.tests.contract.text PRIVATE escape)

# Executable specifications of docs/interface-proposal.md.
# Core expectations are independent of POSIX session behavior.
add_executable(escape.tests.contract
    main.cpp sequence.test.cpp values.test.cpp capabilities.test.cpp
    text.test.cpp parser.test.cpp allocation.test.cpp capability-allocation.test.cpp text-allocation.test.cpp encoding-allocation.test.cpp support/allocations.cpp)
target_link_libraries(escape.tests.contract PRIVATE escape)
target_compile_features(escape.tests.contract PRIVATE cxx_std_20)
target_compile_options(escape.tests.contract PRIVATE -Wall -Wextra -Wpedantic)

foreach(group ENC BUF CAP TXT GLY COL PAR)
    add_test(NAME escape.contract.${group} COMMAND escape.tests.contract "${group}_")
    set_tests_properties(escape.contract.${group} PROPERTIES TIMEOUT 30 RUN_SERIAL TRUE)
endforeach()

if(UNIX)
    add_executable(escape.tests.contract.posix
        main.cpp output.test.cpp reader.test.cpp session.test.cpp failures.test.cpp support/allocations.cpp)
    target_link_libraries(escape.tests.contract.posix PRIVATE escape)
    target_compile_features(escape.tests.contract.posix PRIVATE cxx_std_20)
    target_compile_options(escape.tests.contract.posix PRIVATE -Wall -Wextra -Wpedantic)

    # Private fault injection adapter uses this flag to supply support/faults.hpp's seam.
    # Neither this flag nor that header belongs to the installed public interface.
    target_compile_definitions(escape PRIVATE ESCAPE_CONTRACT_TEST_HOOKS=1)
    target_include_directories(escape PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/support")
    foreach(group OUT IO SES CAP)
        add_test(NAME escape.contract.posix.${group} COMMAND escape.tests.contract.posix "${group}_")
        set_tests_properties(escape.contract.posix.${group} PROPERTIES TIMEOUT 30 RUN_SERIAL TRUE)
    endforeach()
endif()

find_package(Python3 QUIET COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_test(NAME escape.contract.PKG.headers
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/verify_headers.py"
                --compiler "${CMAKE_CXX_COMPILER}" --include "${PROJECT_SOURCE_DIR}/include")
    add_test(NAME escape.contract.PKG.consumers
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/verify_package.py"
                --source "${PROJECT_SOURCE_DIR}" --cmake "${CMAKE_COMMAND}"
                --compiler "${CMAKE_CXX_COMPILER}")
    set_tests_properties(escape.contract.PKG.headers PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
    set_tests_properties(escape.contract.PKG.consumers PROPERTIES TIMEOUT 180 RUN_SERIAL TRUE)
endif()
