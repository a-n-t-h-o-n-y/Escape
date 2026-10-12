"""PKG-01: subproject and installed static-library consumers use the exported target."""

import argparse
import pathlib
import shutil
import subprocess
import tempfile


CONSUMER = r'''#include <esc/esc.hpp>
#include <esc/text.hpp>
int main() {
    if (esc::escape(esc::Cursor{}) != "\x1b[1;1H") return 1;
    if (esc::utf32_to_utf8(U"界") != "\xe7\x95\x8c") return 2;
    esc::Parser parser;
    auto result=parser.next(std::span<char const>{"\x1b[A",3});
    if(result.event != std::optional<esc::Event>{esc::KeyPress{esc::KeyCode::ArrowUp}}) return 3;
    auto options=esc::TerminalOptions::interactive();
    if(options.echo != std::optional{esc::Echo::Off}) return 4;
    esc::Output{}.write(std::string_view{});
    return 0;
}
'''


def run(*command):
    subprocess.run(command, check=True, timeout=90)


def consumer(folder, cmake_text):
    folder.mkdir()
    (folder / "main.cpp").write_text(CONSUMER, encoding="utf-8")
    (folder / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.16)\nproject(Consumer LANGUAGES CXX)\n"
        + cmake_text
        + "\nadd_executable(consumer main.cpp)\ntarget_link_libraries(consumer PRIVATE Escape::escape)\n"
    )


def executable(build):
    for name in ("consumer", "consumer.exe", "Release/consumer.exe"):
        candidate = build / name
        if candidate.is_file():
            return candidate
    raise RuntimeError("Consumer executable was not produced")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True)
    parser.add_argument("--cmake", default="cmake")
    parser.add_argument("--compiler", required=True)
    args = parser.parse_args()
    source = pathlib.Path(args.source).resolve()
    # CMake accepts slash paths quoted in its language; escape quotes/backslashes.
    source_literal = str(source).replace("\\", "/").replace('"', '\\"')
    with tempfile.TemporaryDirectory(prefix="escape-consumers-") as folder:
        root = pathlib.Path(folder)
        subproject = root / "subproject"
        consumer(subproject, 'set(ESCAPE_BUILD_TESTS OFF CACHE BOOL "" FORCE)\n'
                 'set(ESCAPE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)\n'
                 'set(ESCAPE_BUILD_CONTRACT_TESTS OFF CACHE BOOL "" FORCE)\n'
                 f'add_subdirectory("{source_literal}" escape)')
        run(args.cmake, "-S", str(subproject), "-B", str(root / "sub-build"),
            f"-DCMAKE_CXX_COMPILER={args.compiler}", "-DCMAKE_BUILD_TYPE=Release")
        run(args.cmake, "--build", str(root / "sub-build"), "--target", "consumer", "-j", "2", "--config", "Release")
        run(str(executable(root / "sub-build")))
        for linkage, shared in (("static", "OFF"), ("shared", "ON")):
            prefix = root / ("installed-" + linkage)
            library_build = root / ("library-build-" + linkage)
            run(args.cmake, "-S", str(source), "-B", str(library_build),
                f"-DCMAKE_CXX_COMPILER={args.compiler}", "-DCMAKE_BUILD_TYPE=Release", f"-DCMAKE_INSTALL_PREFIX={prefix}",
                "-DESCAPE_BUILD_TESTS=OFF", "-DESCAPE_BUILD_EXAMPLES=OFF",
                "-DESCAPE_BUILD_CONTRACT_TESTS=OFF", f"-DBUILD_SHARED_LIBS={shared}")
            run(args.cmake, "--build", str(library_build), "--target", "escape", "-j", "2", "--config", "Release")
            run(args.cmake, "--install", str(library_build), "--config", "Release")
            installed_consumer = root / ("installed-consumer-" + linkage)
            consumer(installed_consumer, "find_package(Escape CONFIG REQUIRED)")
            run(args.cmake, "-S", str(installed_consumer), "-B", str(root / ("installed-build-" + linkage)),
                f"-DCMAKE_CXX_COMPILER={args.compiler}", "-DCMAKE_BUILD_TYPE=Release", f"-DCMAKE_PREFIX_PATH={prefix}")
            run(args.cmake, "--build", str(root / ("installed-build-" + linkage)), "--target", "consumer", "-j", "2", "--config", "Release")
            binary = executable(root / ("installed-build-" + linkage))
            for dll in (prefix / "bin").glob("*.dll"):
                shutil.copy2(dll, binary.parent / dll.name)
            run(str(binary))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
