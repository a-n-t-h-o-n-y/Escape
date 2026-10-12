"""Exercise Lumen's animation, SGR input, resize, and terminal restoration."""
import argparse
import fcntl
import os
import pty
import select
import struct
import subprocess
import termios
import time


def run(binary, width=100, height=30, quit_key=b"q", synchronize=False):
    master, slave = pty.openpty()
    process = None
    output = bytearray()

    def resize(columns, rows):
        fcntl.ioctl(slave, termios.TIOCSWINSZ,
                    struct.pack("HHHH", rows, columns, 0, 0))

    def until(marker, start=0):
        deadline = time.monotonic() + 5
        while marker not in output[start:] and time.monotonic() < deadline:
            if select.select([master], [], [], 0.05)[0]:
                output.extend(os.read(master, 65536))
            assert process.poll() is None, process.stderr.read()
        assert marker in output[start:], (marker, bytes(output[-300:]))
        return bytes(output[start:])

    def send(keys, marker):
        start = len(output)
        os.write(master, keys)
        return until(marker, start)

    try:
        resize(width, height)
        before = termios.tcgetattr(slave)
        process = subprocess.Popen([binary], stdin=slave, stdout=slave,
                                   stderr=subprocess.PIPE,
                                   env=dict(os.environ, TERM="xterm-ghostty", NO_COLOR="1"))
        if synchronize:
            until(b"\x1b[?2026$p")
            os.write(master, b"\x1b[?2026;2$y")
        until(b" LUMEN" if width > 6 else b"\x1b[38;2;")
        if width > 6:
            until(b"space pause")
            assert "▀".encode() in output
            assert b"\x1b[38;2;" in output and b"\x1b[48;2;" in output
            assert b"\x1b[?1003h" in output and b"\x1b[?1006h" in output
            # Any-motion, press, and wheel events are accepted while animating.
            send(b"\x1b[<35;70;12M\x1b[<0;70;12M\x1b[<65;70;12M", b"space pause")
            send(b" ", b"PAUSED")
            if synchronize:
                assert b"\x1b[?2026h" in output and b"\x1b[?2026l" in output
            send(b" ", b"space pause")
            start = len(output)
            resize(80, 24)
            until(b"\x1b[24;1H", start)
            start = len(output)
            resize(240, 80)
            large = until(b"\x1b[80;1H", start)
            frame = large[large.rfind(b"\x1b[1;1H"):]
            assert b"\x1b[79;1H" in frame
            assert frame.count("▀".encode()) == 239 * 78
            send(b"\x1b[<35;220;70M", b"space pause")
            start = len(output)
            resize(1, 1)
            until(b"\x1b[2J", start)
            start = len(output)
            resize(100, 30)
            until(b"space pause", start)
        os.write(master, quit_key)
        deadline = time.monotonic() + 5
        while process.poll() is None and time.monotonic() < deadline:
            if select.select([master], [], [], 0.05)[0]:
                output.extend(os.read(master, 65536))
        assert process.wait(timeout=1) == 0, process.stderr.read()
        while select.select([master], [], [], 0)[0]:
            output.extend(os.read(master, 65536))
        assert b"\x1b[?1049l" in output and b"\x1b[?25h" in output
        assert b"\x1b[?1003l" in output and b"\x1b[?1006l" in output
        assert termios.tcgetattr(slave) == before
    finally:
        if process is not None:
            if process.poll() is None:
                process.kill()
                process.wait()
            process.stderr.close()
        os.close(master)
        os.close(slave)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--lumen", required=True)
    binary = parser.parse_args().lumen
    run(binary, synchronize=True)
    run(binary, width=4, height=2, quit_key=b"\x03")
    print("PASS Lumen colors, mouse, pause, resize, synchronization, and cleanup")
