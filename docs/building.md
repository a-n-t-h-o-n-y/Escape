# Building Escape

Escape needs a C++20 compiler and CMake 3.16+. It has no third-party library
requirements. Linux and macOS use POSIX terminals; Windows uses native console
handles with VT output. Files and pipes can use `Reader` and `Output` directly.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Python 3 enables header, package, documentation-example, and interactive smoke
checks. The C++ behavior tests also run without Python. For multi-configuration
generators, such as Visual Studio, executable paths include `Release/` and `.exe`.

## Use it as a subproject

```cmake
add_subdirectory(path/to/Escape)
add_executable(my-app main.cpp)
target_link_libraries(my-app PRIVATE Escape::escape)
```

## Install it

```sh
cmake -S . -B build-install -DCMAKE_BUILD_TYPE=Release \
  -DESCAPE_BUILD_TESTS=OFF -DESCAPE_BUILD_EXAMPLES=OFF \
  -DCMAKE_INSTALL_PREFIX=/your/prefix
cmake --build build-install --config Release --parallel
cmake --install build-install --config Release
```

Then use:

```cmake
find_package(Escape CONFIG REQUIRED)
target_link_libraries(my-app PRIVATE Escape::escape)
```

Add the install prefix to `CMAKE_PREFIX_PATH` when it isn't in CMake's search path.

## Build options

| Option | Default | Enables |
| --- | --- | --- |
| `ESCAPE_BUILD_TESTS` | On for the top-level project | Behavior and package tests |
| `ESCAPE_BUILD_EXAMPLES` | On for the top-level project | Tutorials, Lumen, Pong, event-loop example, termcaps |
| `ESCAPE_BUILD_BENCHMARKS` | Off | [Performance runner](../benchmarks/README.md) |
| `ESCAPE_BUILD_DOCS` | Off | [Doxygen reference target](reference.md#generated-reference) |
| `ESCAPE_BUILD_CONTRACT_TESTS` | Off | Also enables the behavior tests |

Tests and examples default off when Escape is added as a subdirectory.
Doxygen is needed only when `ESCAPE_BUILD_DOCS=ON`; Graphviz is not needed.

For test filters, fault injection, and terminal checks, see the
[test guide](../tests/contract/README.md).
