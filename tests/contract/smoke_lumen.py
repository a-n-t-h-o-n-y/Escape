"""Exercise Lumen's animation, SGR input, resize, and terminal restoration."""
import argparse
import fcntl
import os
import pty
import re
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

    def complete_frame(data, rows, marker):
        # A read can also contain the beginning of the next, faster frame.
        marker_at = data.find(marker)
        end = data.rfind(f"\x1b[{rows};1H".encode(), 0, marker_at + len(marker))
        start = data.rfind(b"\x1b[1;1H", 0, end)
        assert start >= 0 and end > start
        return data[start:end]

    def interior_mask(data):
        foreground = background = None
        mask = []
        for style, red, green, blue, pixel in re.findall(
                rb"\x1b\[(38|48);2;(\d+);(\d+);(\d+)m|(\xe2\x96\x80)", data):
            if pixel:
                mask.extend((foreground == (8, 9, 28), background == (8, 9, 28)))
            else:
                color = (int(red), int(green), int(blue))
                if style == b"38":
                    foreground = color
                else:
                    background = color
        assert len(mask) == (width - 1) * (height - 2) * 2
        return mask

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
            images = []
            for key, hint in [(b"1", b"mouse steer | click ripple"),
                              (b"2", b"mouse disturb | click splash"),
                              (b"3", b"mouse steer | click boost"),
                              (b"4", b"drag morph | wheel zoom")]:
                data = send(key, hint)
                frame = complete_frame(data, height, hint)
                images.append(frame[frame.find(b"\x1b[2;1H"):])
                if key == b"4":
                    data = send(b" ", b"PAUSED")
                    original = interior_mask(complete_frame(data, height, b"PAUSED"))
                    send(b" ", b"space pause")
                    os.write(master, b"\x1b[<35;95;28M")
                    time.sleep(0.25)
                    data = send(b" ", b"PAUSED")
                    assert interior_mask(complete_frame(data, height, b"PAUSED")) == original
                    os.write(master, b"\x1b[<0;30;10M\x1b[<32;95;28M\x1b[<0;95;28m")
                    send(b" ", b"space pause")
                    time.sleep(0.35)
                    data = send(b" ", b"PAUSED")
                    assert interior_mask(complete_frame(data, height, b"PAUSED")) != original
                    send(b" ", b"space pause")
                send(b"\x1b[<35;65;14M\x1b[<0;65;14M\x1b[<64;65;14M", hint)
            assert len(set(images)) == 4
            send(b"\t", b"mouse steer | click ripple")
            send(b"\x1b[Z", b"drag morph | wheel zoom")
            send(b"\x1b[<0;22;1M", b"mouse disturb | click splash")
            send(b" ", b"PAUSED")
            send(b"3", b" 3 Warp")
            # A tab change still redraws while animation is paused.
            until(b"PAUSED", len(output) - 100)
            send(b" ", b"mouse steer | click boost")
            send(b"1", b"mouse steer | click ripple")
            start = len(output)
            resize(80, 24)
            until(b"\x1b[24;1H", start)
            start = len(output)
            resize(480, 140)
            large = until(b"\x1b[140;1H", start)
            frame = complete_frame(large, 140, b"\x1b[140;1H")
            assert b"\x1b[139;1H" in frame
            assert frame.count("▀".encode()) == 479 * 138
            send(b"\x1b[<35;450;130M", b"space pause")
            for key, hint in [(b"2", b"mouse disturb"), (b"3", b"click boost"),
                              (b"4", b"drag morph"), (b"1", b"click ripple")]:
                data = send(key, hint)
                frame = complete_frame(data, 140, hint)
                assert b"\x1b[139;1H" in frame
                assert frame.count("▀".encode()) == 479 * 138
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
    print("PASS Lumen distinct scenes, tabs, colors, mouse, pause, resize, synchronization, and cleanup")
