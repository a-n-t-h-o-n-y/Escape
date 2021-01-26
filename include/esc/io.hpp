#ifndef ESC_IO_HPP
#define ESC_IO_HPP
#include <array>
#include <climits>
#include <clocale>
#include <cstddef>
#include <cstdio>
#include <cuchar>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include <esc/esc.hpp>
#include <esc/event.hpp>

namespace esc::io {

// -------------------------- Write to Screen ----------------------------------

/// Writes a single byte to the console via stdout.
inline void write(char c) { std::putchar(c); }

/// Writes a 4 byte char to the console via stdout.
/** Converts the character to a multi-byte array via std::c32rtomb() and sends
 *  each resulting char to write(). The conversion uses the current C locale.
 *  Throws std::runtime_error if there is an error during conversion. */
inline void write(char32_t c) noexcept(false)
{
    auto chars       = std::array<char, MB_LEN_MAX>{};
    auto state       = std::mbstate_t{};
    auto const count = std::c32rtomb(chars.data(), c, &state);
    if (count == std::size_t(-1))
        throw std::runtime_error{"Invalid char32_t to MB conversion."};
    for (auto i = std::size_t{0}; i < count; ++i)
        write(chars[i]);
}

/// Writes each char of \p sv to the console via stdout.
inline auto write(std::string_view const& sv) -> void
{
    for (auto c : sv)
        write(c);
}

/// Writes each char32_t of \p sv to the console via stdout.
/** Calls write(char32_t) for each element of \p sv. */
inline void write(std::u32string_view sv)
{
    for (auto c : sv)
        write(c);
}

/// Send all buffered bytes from calls to write(...) to the console device.
inline void flush() { std::fflush(::stdout); }

// ----------------------------- Reading --------------------------------------

/// Blocking read of a single input Event from stdin.
auto read() -> Event;

/// Read a single input Event from stdin, with timeout.
/** Returns std::nullopt if timeout passes without an Event. */
auto read(int millisecond_timeout) -> std::optional<Event>;

// --------------------------- Init/UnInit -------------------------------------
void disable_canonical_mode_and_echo();

void enable_canonical_mode_and_echo();

[[nodiscard]] inline auto enable_mouse() -> std::string
{
    // 1003 all mouse movement events
    // 1006 extended coordinate SGR mode
    return "\033["
           "?1003;1006h";
    // TODO provide option to enable mouse move events with 1003, 1000 is no
    // mouse move events.
}

[[nodiscard]] inline auto disable_mouse() -> std::string
{
    return "\033["
           "?1003;1006l";
}

/// Sets the locale to utf8, Alt screen buffer, and stdout to use full buffer.
inline void initialize_stdout()
{
    // TODO store current locale in global
    std::setlocale(LC_ALL, "en_US.UTF-8");
    write(esc::alternate_screen_buffer());
    std::setvbuf(stdout, nullptr, _IOFBF, 4'096);
    // TODO sigwinch handler, which somehow makes read() return the resize event
    // you'd want blocking read to return it as well, that'll be tricky.
}

/// Set the screen to the Normal buffer.
inline void uninitialize_stdout()
{
    write(disable_mouse());
    write(esc::normal_screen_buffer());
}

/// Sets up mouse input mode etc...
inline void initialize_stdin()
{
    // TODO should store current termios struct
    write(enable_mouse());
    disable_canonical_mode_and_echo();
}

inline void uninitialize_stdin()
{
    // TODO should reset locale to what it was at init
    // TODO should reset termios to what it was at init
    enable_canonical_mode_and_echo();
}

/// Perform setup, clears the screen.
inline void initialize()
{
    initialize_stdout();
    initialize_stdin();
    flush();
}

/// Performs teardown, reset the screen to the state before initialize() called.
inline void uninitialize()
{
    uninitialize_stdout();
    uninitialize_stdin();
    flush();
}

}  // namespace esc::io
#endif  // ESC_IO_HPP
