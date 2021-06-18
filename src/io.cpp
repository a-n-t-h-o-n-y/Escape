#include <esc/io.hpp>

#include <algorithm>
#include <array>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>

#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <esc/area.hpp>
#include <esc/detail/mb_to_u32.hpp>
#include <esc/detail/u32_to_mb.hpp>
#include <esc/event.hpp>
#include <esc/key.hpp>
#include <esc/terminal.hpp>

#include "detail/tty_file.hpp"

// TODO
// for read
//     auto const fd = open_tty_fd();
//     set_keyboard_mode(MEDIUM_RAW);

//     char byte;
//     ssize_t size = read(fd, &byte, 1);
//     printf("fd3 Read byte   %c\n", byte);

namespace {

/// Return true if there is nothing to ready from stdin.
/** Waits up to \p timeout for bytes to be written to stdin. */
[[nodiscard]] auto is_stdin_empty(int millisecond_timeout) -> bool
{
    auto file   = pollfd{};
    file.fd     = STDIN_FILENO;
    file.events = POLLIN;

    auto const result = poll(&file, 1, millisecond_timeout);

    auto constexpr error   = -1;
    auto constexpr timeout = 0;

    if (result == error) {
        if (errno == EINTR)  // A signal interrupted poll.
            return true;
        throw std::runtime_error{"io.cpp is_stdin_empty(): Poll Error"};
    }
    if (result == timeout)
        return true;
    if (result > 0 && static_cast<bool>(file.revents & POLLIN))
        return false;
    throw std::logic_error{"io.cpp is_stdin_empty(): logic_error."};
}

/// Read a single char from stdin.
auto read_byte() -> char
{
    auto b = char{};
    read(STDIN_FILENO, &b, 1);
    return b;
}

/// Read a single byte, blocking until a byte is read or timeout happens.
/** Returns std::nullopt on timeout. */
auto read_byte(int millisecond_timeout) -> std::optional<char>
{
    if (!is_stdin_empty(millisecond_timeout))
        return read_byte();
    return std::nullopt;
}

// Mouse -----------------------------------------------------------------------

auto previous_mouse_btn = esc::Mouse::Button{};

// SIGWINCH --------------------------------------------------------------------

/// Notifies read() and next_state(Initial) that the window has been resized.
bool window_resize_sig = false;

/// Set the window_resize_sig flag to true on SIGWINCH signals.
extern "C" void resize_handler(int sig)
{
    if (sig == SIGWINCH)
        window_resize_sig = true;
}

// Token -----------------------------------------------------------------------

class Control_sequence {
   public:
    std::string parameter_bytes;
    std::string intermediate_bytes;
    char final_byte;

   public:
    explicit Control_sequence(std::string sequence)
    {
        if (sequence.empty())
            throw std::runtime_error{"Control_sequence(): Invalid Input"};
        std::copy_if(std::cbegin(sequence), std::cend(sequence),
                     std::back_inserter(parameter_bytes),
                     [](char c) { return c >= '\x30' && c <= '\x3F'; });
        std::copy_if(std::cbegin(sequence), std::cend(sequence),
                     std::back_inserter(intermediate_bytes),
                     [](char c) { return c >= '\x20' && c <= '\x2F'; });
        final_byte = sequence.back();
    }
};

struct Escaped {
    char character;
};

struct UTF8 {
    std::array<char, 4> bytes;
};

struct Window {};

using Token = std::variant<Control_sequence, Escaped, UTF8, Window>;

// -----------------------------------------------------------------------------

/// Retrieves the integer type parameter at \p index, returns a type T.
/** \p bytes should only be the parameter bytes. */
template <typename T, std::size_t index>
auto parameter(std::string bytes) -> T
{
    if constexpr (index != 0) {  // silence gcc warning
        for (auto count = 0uL; count < index; ++count)
            bytes = bytes.substr(bytes.find(';', 0) + 1);
    }

    return std::stoul(bytes.substr(0, bytes.find(';', 0)));
}

auto parse_mouse(Control_sequence cs) -> esc::Event
{
    auto const is_sgr = cs.parameter_bytes.front() == '<';
    auto const btn    = [&] {
        // SGR Mode
        if (is_sgr) {
            cs.parameter_bytes = cs.parameter_bytes.substr(1);
            return parameter<int, 0>(cs.parameter_bytes);
        }
        // URXVT Mode
        return parameter<int, 0>(cs.parameter_bytes) - 32;
    }();
    auto const at = esc::Point{parameter<int, 1>(cs.parameter_bytes) - 1,
                               parameter<int, 2>(cs.parameter_bytes) - 1};

    // State
    using esc::Mouse;
    auto mouse = Mouse{at, Mouse::Button::None, {}};

    // Modifiers
    auto constexpr shift = 0b00000100;
    auto constexpr alt   = 0b00001000;
    auto constexpr ctrl  = 0b00010000;

    mouse.modifiers.shift = btn & shift;
    mouse.modifiers.alt   = btn & alt;
    mouse.modifiers.ctrl  = btn & ctrl;

    // Button
    auto constexpr btn_mask = 0b11000011;
    mouse.button            = static_cast<Mouse::Button>(btn & btn_mask);

    // Move Event
    auto constexpr move_event = 0b00100000;
    if (btn & move_event)
        return esc::Mouse_move{mouse};

    // Scroll Event
    if (mouse.button == Mouse::Button::ScrollUp ||
        mouse.button == Mouse::Button::ScrollDown) {
        return esc::Scroll_wheel{mouse};
    }

    // Release Event
    if (cs.final_byte == 'm')
        return esc::Mouse_release{mouse};

    // urxvt btn release does not mention the button that was released.
    if (!is_sgr && mouse.button == Mouse::Button::None) {
        mouse.button = previous_mouse_btn;
        return esc::Mouse_release{mouse};
    }

    // Press Event
    previous_mouse_btn = mouse.button;
    if (cs.final_byte == 'M')
        return esc::Mouse_press{mouse};

    throw std::runtime_error{"io.cpp::parse_mouse(): Bad Mouse Event Parse"};
}

auto parameter_count(std::string parameter_bytes) -> std::size_t
{
    if (parameter_bytes.empty())
        return 0;
    return std::count(std::cbegin(parameter_bytes), std::cend(parameter_bytes),
                      ';') +
           1;
}

auto parse_tilde(std::string parameter_bytes) -> esc::Key
{
    if (parameter_count(parameter_bytes) == 0)
        throw std::runtime_error{"io.cpp: parse_tilde: No Parameter Bytes"};
    return static_cast<esc::Key>(127 + parameter<int, 0>(parameter_bytes));
}

auto parse_key(Control_sequence cs) -> esc::Key
{
    switch (cs.final_byte) {
        using Key = esc::Key;
        case 'A': return Key::Arrow_up;
        case 'B': return Key::Arrow_down;
        case 'C': return Key::Arrow_right;
        case 'D': return Key::Arrow_left;
        case 'E': return Key::Begin;
        case 'F': return Key::End;
        case 'G': return Key::Page_down;
        case 'H': return Key::Home;
        case 'I': return Key::Page_up;
        case 'L': return Key::Insert;
        case 'P': return Key::Function1;
        case 'Q': return Key::Function2;
        case 'R': return Key::Function3;
        case 'S': return Key::Function4;
        case 'Z': return Key::Back_tab;
        case '~': return parse_tilde(cs.parameter_bytes);
    }
    throw std::runtime_error{"io.cpp parse_key(): Unknown final_byte: " +
                             std::string(1, cs.final_byte)};
}

auto parse_key_modifiers(std::string parameter_bytes) -> esc::Mod
{
    if (parameter_count(parameter_bytes) < 2)
        return static_cast<esc::Mod>(0);
    auto const mod = parameter<int, 1>(parameter_bytes);
    switch (mod) {
        using esc::Mod;
        case 2: return Mod::Shift;
        case 3: return Mod::Alt;
        case 4: return Mod::Shift | Mod::Alt;
        case 5: return Mod::Ctrl;
        case 6: return Mod::Shift | Mod::Ctrl;
        case 7: return Mod::Ctrl | Mod::Alt;
        case 8: return Mod::Shift | Mod::Ctrl | Mod::Alt;
        case 9: return Mod::Meta;
        case 10: return Mod::Shift | Mod::Meta;
        case 11: return Mod::Alt | Mod::Meta;
        case 12: return Mod::Shift | Mod::Alt | Mod::Meta;
        case 13: return Mod::Ctrl | Mod::Meta;
        case 14: return Mod::Shift | Mod::Ctrl | Mod::Meta;
        case 15: return Mod::Ctrl | Mod::Alt | Mod::Meta;
        case 16: return Mod::Shift | Mod::Ctrl | Mod::Alt | Mod::Meta;
    }
    throw std::runtime_error{"io.cpp parse_key_modifiers(): Unknown Mod: " +
                             std::to_string(mod)};
}

auto parse(Control_sequence const& cs) -> esc::Event
{
    if (cs.final_byte == 'M' || cs.final_byte == 'm')
        return parse_mouse(cs);
    return esc::Key_press{parse_key(cs) |
                          parse_key_modifiers(cs.parameter_bytes)};
}

auto parse(Escaped e) -> esc::Event
{
    return esc::Key_press{static_cast<esc::Key>(e.character)};
}

auto parse(UTF8 x) -> esc::Event
{
    return esc::Key_press{esc::char32_to_key(esc::detail::mb_to_u32(x.bytes))};
}

auto parse(Window) -> esc::Event
{
    return esc::Window_resize{esc::terminal_area()};
}

// Lexer -----------------------------------------------------------------------

struct Initial {};

struct Final {
    Token result;
};

struct Escape {};

struct Maybe_escaped {};

struct Maybe_CSI {
    char c;  // The possibly escaped char.
};

struct CSI {
    std::string value;
};

using Lexer =
    std::variant<Initial, Final, Escape, Maybe_escaped, Maybe_CSI, CSI, UTF8>;

// -----------------------------------------------------------------------------

auto constexpr escape = '\033';

auto next_state(Initial) -> Lexer
{
    auto constexpr timeout = 30;  // milliseconds
    while (true) {
        if (window_resize_sig) {
            window_resize_sig = false;
            return Final{Window{}};
        }
        if (auto const b = read_byte(timeout); b.has_value()) {
            if (*b == escape)
                return Escape{};
            else
                return UTF8{{*b}};
        }
    }
}

auto next_state(Final f) -> Lexer { return f; }

auto next_state(Escape) -> Lexer
{
    if (is_stdin_empty(0))
        return Final{UTF8{{escape}}};
    else
        return Maybe_escaped{};
}

auto next_state(Maybe_escaped) -> Lexer
{
    auto const c = read_byte();
    if (c != '[' && c != 'O')
        return Final{Escaped{c}};
    else
        return Maybe_CSI{c};
}

auto next_state(Maybe_CSI state) -> Lexer
{
    if (is_stdin_empty(0))
        return Final{Escaped{state.c}};
    else
        return CSI{""};
}

auto next_state(CSI const& state) -> Lexer
{
    auto const b = read_byte();
    if (b >= 0x40 && b <= 0x7E)
        return Final{Control_sequence{state.value + b}};
    else
        return CSI{state.value + std::string(1, b)};
}

// Returns 1 if all set bits in \p mask match up with set bits in value, zeros
// in the mask are treated as wildcards and can be anything in \p value.
auto matches_mask(std::uint8_t mask, char value) -> std::uint8_t
{
    return static_cast<std::uint8_t>((mask & value) == mask);
}

/// Returns the number of bytes left to read to form a complete utf8 character.
/** Should be branchless. */
auto bytes_left_to_read(char first) -> std::uint8_t
{
    // utf8 initial bytes
    auto constexpr mask_1 = std::uint8_t{0b11000000};
    auto constexpr mask_2 = std::uint8_t{0b11100000};
    auto constexpr mask_3 = std::uint8_t{0b11110000};

    auto count = matches_mask(mask_1, first);
    count += matches_mask(mask_2, first);
    count += matches_mask(mask_3, first);
    return count;
}

auto next_state(UTF8 state) -> Lexer
{
    auto index = 1;
    for (auto count = bytes_left_to_read(state.bytes[0]); count != 0; --count)
        state.bytes[index++] = read_byte();
    return Final{state};
}

// -----------------------------------------------------------------------------

/// Read a single input command from the terminal.
auto read_single_token() -> Token
{
    auto state = Lexer{Initial{}};
    while (!std::holds_alternative<Final>(state))
        state = std::visit([](auto s) { return next_state(s); }, state);
    return std::get<Final>(state).result;
}

}  // namespace

namespace esc {

void write(char c) { std::putchar(c); }

void write(char32_t c) noexcept(false)
{
    auto const [count, chars] = esc::detail::u32_to_mb(c);
    for (auto i = std::size_t{0}; i < count; ++i)
        write(chars[i]);
}

void write(std::string_view sv)
{
    for (auto c : sv)
        write(c);
}

void write(std::string const& s) { std::fputs(s.c_str(), stdout); }

void write(char const* s) { std::fputs(s, stdout); }

void write(std::u32string_view sv)
{
    for (auto c : sv)
        write(c);
}

void flush() { std::fflush(::stdout); }

// you want to read from console and stdin, which will have duplicates and stdin
// will throw out keyboard events, but how do you separate this out and read as
// many times as needed? just always check console first and if there is nothing
// move onto stdin and if there are events in both, you will read from console
// and return and from stdin and return, but this returns an Event, not an
// optional, and that second read is going to be thrown out.. maybe you can do a
// duplicate check somehow.. like if you are reading from ... console is going
// to have two events for every single key event from stdin. so you can't just
// ignore stdin for each event. maybe take a step back and think about the
// higher level. Read from two inputs, some will be thrown out, and you want to
// timeout so you can check if there is an event ready, that won't be thrown
// out.

// you should have two separate sections, you have to handle this from a higher
// level, but you have the functions you need to work with.

// you have a timeout function that checks if there is input, one function for
// each fd.
// then you have a read function for both, so that when there is input, you let
// it read an event from that particular fd read_stdin() read_tty() and these
// both return events.

// then at the callsite of read you determine if you want to use the mouse
// events from stdin or not, and if you event want to call to the tty() version.

// so where is read() called from? might be in in termox.

// so the user of read() has to now call read_stdin() and read_tty() ?

// so you have these is something ready and read functions, and you can use them
// at the proper call site, you will have to enable it in escape, initialize it,
// and you will have to also know at termox level to throw own and to not call
// tty

// auto read_stdin() -> Event;

// auto read_tty() -> Event;

auto read() -> Event
{
    auto const token = read_single_token();
    return std::visit([](auto t) { return parse(t); }, token);
}

auto read(int millisecond_timeout) -> std::optional<Event>
{
    // TODO if special up/down mode is on, check if console fd is not empty too
    if (window_resize_sig || !is_stdin_empty(millisecond_timeout))
        return esc::read();
    return std::nullopt;
}

}  // namespace esc

namespace esc::detail {

void register_SIGWINCH()
{
    if (std::signal(SIGWINCH, &resize_handler) == SIG_ERR)
        throw std::runtime_error{"register_SIGWINCH(): std::signal call"};
}

}  // namespace esc::detail
