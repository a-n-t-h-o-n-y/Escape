"""PKG-01: each proposed public header stands alone without private dependencies."""

import argparse
import pathlib
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--include", required=True)
    args = parser.parse_args()
    headers = (
        "color", "trait", "brush", "point", "area", "sequence", "text", "glyph",
        "capabilities", "event", "parser", "io", "terminal", "esc",
    )
    forbidden = ("/zzz/", "/unicode/", "/termios.h", "/sys/ioctl.h", "/linux/kd.h")
    failures = []
    with tempfile.TemporaryDirectory(prefix="escape-headers-") as folder:
        source = pathlib.Path(folder) / "header.cpp"
        for header in headers:
            source.write_text(f"#include <esc/{header}.hpp>\nint main() {{}}\n")
            result = subprocess.run(
                [args.compiler, "-std=c++20", "-fsyntax-only", "-H", "-I", args.include, str(source)],
                capture_output=True, text=True, timeout=20,
            )
            leaked = [line for line in result.stderr.splitlines() if any(name in line for name in forbidden)]
            if result.returncode or leaked:
                failures.append(header)
                print(f"FAIL esc/{header}.hpp\n{result.stderr}")
            else:
                print(f"PASS esc/{header}.hpp")
        if not failures:
            # Guard API boundaries separately from runtime behavior. These fail
            # only after the positive header checks establish that the API exists.
            invalid_calls = {
                "integer command": "esc::escape(42)",
                "unwrapped point": "esc::escape(esc::Point{})",
                "bare mouse mode": "esc::escape(esc::MouseMode::Basic)",
                "OS setting as bytes": "esc::escape(esc::Echo::Off)",
                "integer output": "esc::write(42)",
                "boolean output": "esc::write(true)",
                "isolated UTF8 unit": "char8_t{'a'} | esc::Trait::Bold",
                "isolated UTF16 unit": "char16_t{'a'} | esc::Trait::Bold",
                "mode command through send": "terminal.send(esc::ScreenBuffer::Alternate)",
            }
            for name, expression in invalid_calls.items():
                source.write_text(
                    "#include <esc/esc.hpp>\n"
                    f"void check(esc::Terminal& terminal) {{ (void)({expression}); }}\n"
                )
                result = subprocess.run(
                    [args.compiler, "-std=c++20", "-fsyntax-only", "-I", args.include, str(source)],
                    capture_output=True, text=True, timeout=20,
                )
                if result.returncode == 0:
                    failures.append(name)
                    print(f"FAIL rejected-call contract: {name} unexpectedly compiles")
                else:
                    print(f"PASS rejected-call contract: {name}")
    return bool(failures)


if __name__ == "__main__":
    raise SystemExit(main())
