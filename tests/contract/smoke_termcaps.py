"""Exercise the tester against a PTY; assertions concern bytes, not audible playback."""
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


class Session:
    def __init__(self, binary, width=100, height=30, no_color=False):
        self.closed = False
        self.master, self.slave = pty.openpty()
        self.resize(width, height)
        self.before = termios.tcgetattr(self.slave)
        env = dict(os.environ, TERM="xterm-256color", COLORTERM="truecolor")
        env.pop("NO_COLOR", None)
        if no_color:
            env["NO_COLOR"] = "1"
        self.process = subprocess.Popen([binary], stdin=self.slave, stdout=self.slave,
                                        stderr=subprocess.PIPE, env=env)
        self.output = bytearray()
        try:
            self.until(b"Page: System palette")
        except BaseException:
            self.dispose()
            raise

    def resize(self, width, height):
        fcntl.ioctl(self.slave, termios.TIOCSWINSZ,
                    struct.pack("HHHH", height, width, 0, 0))

    def until(self, marker, start=0):
        deadline = time.monotonic() + 4
        while marker not in self.output[start:] and time.monotonic() < deadline:
            if select.select([self.master], [], [], 0.05)[0]:
                try:
                    data = os.read(self.master, 65536)
                except OSError as e:
                    if e.errno != errno.EIO:
                        raise
                    data = b""
                self.output.extend(data)
            if self.process.poll() is not None:
                break
        assert marker in self.output[start:], (marker, bytes(self.output[-1600:]))
        return bytes(self.output[start:])

    def send(self, keys, marker):
        start = len(self.output)
        os.write(self.master, keys)
        return self.until(marker, start)

    def close(self):
        try:
            output = self.send(b"q", b"visual observations")
            self.process.wait(timeout=4)
            assert self.process.returncode == 0, self.process.stderr.read()
            assert b"\x1b[?25h" in self.output and b"\x1b[?1049l" in self.output
            assert termios.tcgetattr(self.slave) == self.before
            return output
        finally:
            self.dispose()

    def dispose(self):
        if self.closed:
            return
        self.closed = True
        if self.process.poll() is None:
            self.process.kill()
            self.process.wait()
        self.process.stderr.close()
        os.close(self.master)
        os.close(self.slave)


def main(binary):
    plain = subprocess.run([binary, "--report"], capture_output=True,
                           env=dict(os.environ, TERM="malicious\x1b[31m"), timeout=4)
    assert plain.returncode == 0 and b"\x1b" not in plain.stdout
    assert b"malicious\\x1B[31m" in plain.stdout
    assert b"DECPS notes" in plain.stdout and b"assumptions, not test results" in plain.stdout
    assert subprocess.run([binary, "--help"], capture_output=True, timeout=4).returncode == 0
    assert subprocess.run([binary, "--invalid"], capture_output=True, timeout=4).returncode == 1
    redirected = subprocess.run([binary], input=b"", capture_output=True, timeout=4)
    assert redirected.returncode == 0 and b"\x1b" not in redirected.stdout

    s = Session(binary)
    try:
        assert b"\x07" not in s.output and b",~" not in s.output
        s.send(b"y", b"Recorded your visual verdict")
        s.send(b"2", b"256-COLOR PALETTE")
        s.send(b"3", b"RGB / TRUE COLOR")
        assert b"\x1b[48;2;" in s.output
        s.send(b"4", b"TEXT ATTRIBUTES")
        s.send(b"5", b"UNICODE / CELL ALIGNMENT")
        s.send(b"6", b"LIVE INPUT")
        s.send(b"mfv", b"v paste ON")
        s.send(b"\x1b[<0;10;11M", b"Mouse press button 1 at 9,10")
        s.send(b"\x1b[I", b"Focus gained")
        s.send(b"\x1b[200~hello\x1b[31m\x1b[201~", b"hello\\x1B[31m")
        s.send(b"7s", b"PASS")
        s.send(b"e", b"Expect AC!D")
        assert b"\x1b[1P" in s.output and b"\x1b[1@" in s.output
        s.send(b"r", b"Expect row 2")
        s.send(b"c", b"Requested cursor shape")
        s.send(b"8", b"OSC 8 HYPERLINKS")
        assert b"\x1b]8;;https://" in s.output
        s.send(b"9", b"BELL / DECPS SOUND")
        s.send(b"b", b"Bell requested")
        assert b"\x07" in s.output
        s.send(b"n", b"DECPS chime sent")
        assert b"\x1b[4;4;1;5;8;13,~" in s.output
        s.send(b"p", b"\x1b[?2026$p")
        s.send(b"\x1b[?2026;2$y", b"Mode 2026: reset")
        assert b"No reply (timeout)" in s.output  # Other queries stayed unanswered.
        s.send(b"t", b"\x1b[c")
        s.send(b"\x1b[?1;2c", b"Device attributes: 1 2")
        s.send(b"k", b"\x1b[?u")
        s.send(b"\x1b[?0u", b"\x1b[=31;1u")
        s.send(b"\x1b[?31u", b"Kitty flags: 31")
        s.send(b"a", b"ENVIRONMENT ASSUMPTIONS")
        s.resize(48, 18)
        s.until(b"Resize to at least 70 x 24", len(s.output))
        s.resize(70, 24)
        s.until(b"70 x 24", len(s.output))
        s.send(b"2\x1b[B\x1b[B\x1b[B\x1b[B\x1b[B\x1b[B\x1b[B\x1b[B\x1b[B\x1b[B\x1b[B\x1b[B\x1b[B\x1b[B\x1b[B",
               b"\x1b[48;5;255m")
        output = s.close()
        s = None
        assert b"System palette: pass" in output
        assert b"\x1b[=0;1u" in output  # Restore the captured keyboard flags.
    finally:
        if s is not None:
            s.dispose()
    s = Session(binary, no_color=True)
    try:
        assert b"\x1b[36m" not in s.output  # NO_COLOR applies to interface chrome.
        s.send(b"3", b"\x1b[48;2;")        # Test samples still request actual RGB.
        s.close()
        s = None
    finally:
        if s is not None:
            s.dispose()
    print("PASS termcaps interactive pages, samples, input, resize, cleanup, and plain report")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--termcaps", required=True)
    main(parser.parse_args().termcaps)
