#pragma once
#include <poll.h>
#include <sys/types.h>
#include <termios.h>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <esc/area.hpp>
#include <esc/capabilities.hpp>

namespace esc::detail {
auto read_bytes(int fd, void* buffer, std::size_t count) -> ssize_t;
auto poll_input(struct pollfd* descriptors, nfds_t count, int timeout) -> int;
auto write_bytes(std::FILE* stream, char const* bytes, std::size_t count)
    -> std::size_t;
auto flush_stream(std::FILE* stream) -> int;
auto get_attributes(int fd, struct termios* settings) -> int;
auto set_attributes(int fd, struct termios const* settings) -> int;
auto get_size(int fd) -> Area;
auto environment() -> Environment;
using Clock = std::chrono::steady_clock;
auto deadline_after(Clock::time_point start, std::chrono::milliseconds duration)
    -> Clock::time_point;
auto poll_delay(Clock::time_point deadline) -> int;
}  // namespace esc::detail
