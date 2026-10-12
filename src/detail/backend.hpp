#pragma once
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <esc/area.hpp>
#include <esc/capabilities.hpp>
#include <esc/terminal.hpp>
#include <utility>
#ifdef _WIN32
#    include <memory>
#    include <string>
#else
#    include <poll.h>
#    include <termios.h>
#endif

namespace esc::detail {
#ifdef _WIN32
struct Attributes {
    unsigned long input_mode = 0, output_mode = 0;
    unsigned input_codepage = 0, output_codepage = 0;
    int output = -1, output_crt_mode = -1;
};
#else
using Attributes = termios;
#endif
struct Input {
    int descriptor;
    bool terminal = false, hung_up = false;
#ifdef _WIN32
    struct ConsoleRead;
    std::unique_ptr<ConsoleRead> console_read;
    std::string pending, line;
    InputProtocols protocols{};
    int output_descriptor = -1;
    MouseMode mouse_mode = MouseMode::Move;
    unsigned long buttons = 0;
    std::size_t offset = 0;
    char16_t high_surrogate = 0;
    unsigned short repeat = 0;
    std::string repeated;
#endif
    explicit Input(int fd);
    ~Input();
};
auto descriptor(std::FILE* stream) -> int;
auto is_terminal(int fd) -> bool;
auto terminal_identity(int input, int output) -> std::uint64_t;
auto read_bytes(Input& input, void* buffer, std::size_t count) -> std::ptrdiff_t;
auto poll_input(Input& input, int timeout) -> int;
auto wait_source(Input& input) -> WaitSource;
auto write_bytes(std::FILE* stream, char const* bytes, std::size_t count)
    -> std::size_t;
auto flush_stream(std::FILE* stream) -> int;
auto get_attributes(int input, int output, Attributes* settings) -> int;
auto set_attributes(int input, Attributes const* settings) -> int;
auto desired_attributes(Attributes captured, TerminalOptions const& options)
    -> Attributes;
auto query_attributes(Attributes captured) -> Attributes;
void prepare_queries(Input& input);
void stop_queries(Input& input) noexcept;
auto transport_setup_required() noexcept -> bool;
void begin_session(Attributes& captured);
auto get_size(int fd) -> Area;
auto environment() -> Environment;
auto session_profile(int input, int output) -> ProfileReport;
using Clock = std::chrono::steady_clock;
auto deadline_after(Clock::time_point start, std::chrono::milliseconds duration)
    -> Clock::time_point;
auto poll_delay(Clock::time_point deadline) -> int;
}  // namespace esc::detail

#ifdef ESCAPE_CONTRACT_TEST_HOOKS
#    include "faults.hpp"
namespace esc::testing {
auto intercept(Operation operation, std::size_t count) -> std::pair<bool, std::size_t>;
void transferred(Operation operation, std::size_t count);
}  // namespace esc::testing
#    define ESC_INTERCEPT(op, count) \
        esc::testing::intercept(esc::testing::Operation::op, count)
#else
#    define ESC_INTERCEPT(op, count) \
        std::pair<bool, std::size_t> { false, count }
#endif
