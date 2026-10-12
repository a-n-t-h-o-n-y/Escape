"""Check documentation links, code fences, and runnable tutorial behavior."""
import argparse
import os
from pathlib import Path
import re
import subprocess
import sys
from urllib.parse import unquote

sys.dont_write_bytecode = True
from smoke_examples import run


def check_sources(root):
    excluded = {".git", "build", ".cache"}
    checked = examples = 0
    for path in root.rglob("*.md"):
        if excluded.intersection(path.relative_to(root).parts):
            continue
        text = path.read_text()
        for target in re.findall(r"\[[^\]]*\]\(([^)]+)\)", text):
            target = target.strip("<>")
            if re.match(r"[a-zA-Z][a-zA-Z0-9+.-]*:", target):
                continue
            relative = unquote(target.split("#", 1)[0])
            if relative:
                assert (path.parent / relative).exists(), (path, target)
                checked += 1
        for source, code in re.findall(
                r"<!-- example: (.*?) -->\s*```cpp\n(.*?)\n```", text, re.S):
            assert code == (root / source).read_text().rstrip("\n"), (path, source)
            examples += 1
    assert examples == 3, examples
    print(f"PASS {checked} local links and {examples} compiled code examples")


def plain_output(binary):
    result = subprocess.run([binary], capture_output=True, timeout=5,
                            env=dict(os.environ, TERM="xterm-256color", NO_COLOR="1"))
    assert result.returncode == 0, result.stderr
    # NO_COLOR preserves bold/dim, so remove SGR before comparing text.
    assert b"38;" not in result.stdout and b"48;" not in result.stdout
    return re.sub(rb"\x1b\[[0-9;]*m", b"", result.stdout).decode()


def main():
    parser = argparse.ArgumentParser()
    for name in ("source", "output", "layout", "picker"):
        parser.add_argument("--" + name, required=True)
    args = parser.parse_args()
    check_sources(Path(args.source))
    assert plain_output(args.output) == "Built 42 files successfully\n"
    assert plain_output(args.layout) == (
        "Name      Status\nCafé      ready\n東京      ready\ne\u0301clair    ready\n")
    print("PASS styled output and Unicode table alignment")
    run(args.picker, b"\x1b[B\x1b[B\r", expected=b"Selected Production")
    run(args.picker, b"\x1b[A\r", expected=b"Selected Production")
    run(args.picker, b"q", expected=b"Choose a destination")


if __name__ == "__main__":
    main()
