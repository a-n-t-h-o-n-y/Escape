"""Run migrated example applications against a PTY, never the user's terminal."""
import argparse
import errno
import fcntl
import os
import pty
import select
import struct
import subprocess
import termios
import time


def run(binary, keys, width=100, height=30, expected=None):
    master, slave = pty.openpty()
    try:
        fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack("HHHH", height, width, 0, 0))
        before = termios.tcgetattr(slave)
        env = dict(os.environ, TERM="xterm-256color", COLORTERM="truecolor")
        env.pop("NO_COLOR", None)
        process = subprocess.Popen([binary], stdin=slave, stdout=slave,
                                   stderr=subprocess.PIPE, env=env)
        try:
            output = bytearray()
            sent = False
            deadline = time.monotonic() + 8
            while time.monotonic() < deadline:
                if select.select([master], [], [], 0.05)[0]:
                    try:
                        chunk = os.read(master, 65536)
                    except OSError as error:
                        if error.errno != errno.EIO:
                            raise
                        chunk = b""
                    output.extend(chunk)
                # Wait for the actual startup mode change rather than a sleep.
                if not sent and b"\x1b[?25l" in output:
                    os.write(master, keys)
                    sent = True
                if process.poll() is not None:
                    while select.select([master], [], [], 0)[0]:
                        try:
                            chunk = os.read(master, 65536)
                        except OSError:
                            break
                        if not chunk:
                            break
                        output.extend(chunk)
                    break
            else:
                raise AssertionError("example did not terminate before its deadline")
            assert process.returncode == 0, process.stderr.read().decode(errors="replace")
            assert sent
            if expected is not None:
                assert expected in output
            assert b"\x1b[?1049h" in output
            assert b"\x1b[?25h" in output and b"\x1b[?1049l" in output
            assert termios.tcgetattr(slave) == before
            print("PASS", binary)
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()
            process.stderr.close()
    finally:
        os.close(master)
        os.close(slave)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--termcaps", required=True)
    parser.add_argument("--pong", required=True)
    parser.add_argument("--event-loop", required=True)
    args = parser.parse_args()
    run(args.termcaps, b"q")
    run(args.pong, b"\r\x1b[B\x1b[B\r")
    play_and_quit = b"\r\r\r\r\r\x1b\x1b[B\x1b[B\r"
    run(args.pong, play_and_quit, width=81, height=25,
        expected=b"Resize to at least 83 x 29 to play")
    run(args.pong, play_and_quit, width=83, height=29,
        expected=b"Press Enter to start next round")

    run(args.event_loop, b"q")
