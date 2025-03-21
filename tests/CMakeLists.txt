# Unit Tests
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
        zzz
)

message(STATUS "AddressSanitizer Enabled for escape.tests.unit")
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(escape.tests.unit
        PRIVATE
            -fsanitize=address
            -fno-omit-frame-pointer
    )
    target_link_options(escape.tests.unit
        PRIVATE
            -fsanitize=address
    )
elseif (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_compile_options(escape.tests.unit
        PRIVATE
            /fsanitize=address
    )
    target_link_options(escape.tests.unit
        PRIVATE
            /INCREMENTAL:NO
            /DEBUG
            /fsanitize=address
    )
endif()