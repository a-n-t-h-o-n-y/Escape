#include <esc/io.hpp>

#include <algorithm>
#include <iterator>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>

#include <iostream>  //temp

#include <poll.h>
#include <termios.h>
#include <unistd.h>

#include <esc/debug.hpp>
#include <esc/event.hpp>
#include "esc/key.hpp"

namespace {

/// Return true if there is nothing to ready from stdin.
/** Waits up to \p timeout for bytes to be written to stdin. */
auto is_stdin_empty(int millisecond_timeout) -> bool
{
    auto file   = pollfd{};
    file.fd     = STDIN_FILENO;
    file.events = POLLIN;

    auto const result = poll(&file, 1, millisecond_timeout);

    if (result == -1)
        throw std::runtime_error{"Error in is_stdin_empty(...);"};
    if (result == 0)  // timeout
        return true;
    if (result > 0 && (file.revents & POLLIN))
        return false;
    throw std::logic_error{"is_stdin_empty() logic_error."};
}

/// Read a single char from stdin.
auto read_char() -> char
{
    auto c = char{};
    read(STDIN_FILENO, &c, 1);
    return c;
}

// Token -----------------------------------------------------------------------

class Control_sequence {
   public:
    std::string parameter_bytes;
    std::string intermediate_bytes;
    char final_byte;

   public:
    Control_sequence(std::string sequence)
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

using Token = std::variant<Control_sequence, Escaped, char>;

// -----------------------------------------------------------------------------

/// Retrieves the parameter at \p index, read at type T.
/** \p bytes should only be the parameter bytes. */
template <typename T, std::size_t index>
auto parameter(std::string bytes) -> T
{
    for (auto count = 0uL; count < index; ++count)
        bytes = bytes.substr(bytes.find(';', 0) + 1);

    auto ss     = std::stringstream{bytes.substr(0, bytes.find(';', 0))};
    auto result = T{};
    ss >> result;
    return result;
}

auto parse_mouse(Control_sequence cs) -> esc::Event
{
    using esc::Mouse;
    auto const parameter_bytes = cs.parameter_bytes.substr(1);

    auto const btn = parameter<int, 0>(parameter_bytes);
    auto const at  = esc::Point{parameter<std::size_t, 1>(parameter_bytes) - 1,
                               parameter<std::size_t, 2>(parameter_bytes) - 1

    };

    // State
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

    mouse.button = static_cast<Mouse::Button>(btn & btn_mask);

    // Move Event
    auto constexpr move_event = 0b00100000;
    if (btn & move_event)
        return esc::Mouse_move{mouse};

    // Scroll Event
    if (mouse.button == Mouse::Button::ScrollUp ||
        mouse.button == Mouse::Button::ScrollDown) {
        return esc::Scroll_wheel{mouse};
    }

    // Press Event
    if (cs.final_byte == 'M')
        return esc::Mouse_press{mouse};

    // Release Event
    if (cs.final_byte == 'm')
        return esc::Mouse_release{mouse};
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
        return esc::Key::Invalid;
    auto const param = parameter<int, 0>(parameter_bytes);
    // TODO this could be a cast to key if you lay it out correctly at some
    // offset with gaps.
    switch (param) {
        using Key = esc::Key;
        case 1: return Key::Home;
        case 2: return Key::Insert;
        case 3: return Key::Delete;
        case 4: return Key::End;
        case 5: return Key::Page_up;
        case 6: return Key::Page_down;
        case 15: return Key::Function5;
        case 17: return Key::Function6;
        case 18: return Key::Function7;
        case 19: return Key::Function8;
        case 20: return Key::Function9;
        case 21: return Key::Function10;
        case 23: return Key::Function11;
        case 24: return Key::Function12;
    }
    throw std::runtime_error{"io.cpp parse_tilde(): Unknown param: " +
                             std::to_string(param)};
}

auto parse_key(Control_sequence cs) -> esc::Key
{
    switch (cs.final_byte) {
        using Key = esc::Key;
        case 'A': return Key::Arrow_up;
        case 'B': return Key::Arrow_down;
        case 'C': return Key::Arrow_right;
        case 'D': return Key::Arrow_left;
        case 'H': return Key::Home;
        case 'F': return Key::End;
        case 'P': return Key::Function1;
        case 'Q': return Key::Function2;
        case 'R': return Key::Function3;
        case 'S': return Key::Function4;
        case 'E': return Key::Begin;
        case '~': return parse_tilde(cs.parameter_bytes);
    }
    throw std::runtime_error{"io.cpp parse_key(): Unknown final_byte: " +
                             std::string(1, cs.final_byte)};
}

auto parse_key_modifiers(std::string parameter_bytes)
    -> esc::Key_press::Modifiers
{
    if (parameter_count(parameter_bytes) < 2)
        return {};
    auto const mod = parameter<int, 1>(parameter_bytes);
    switch (mod) {
        // {shift, ctrl, alt, meta}
        case 2: return {true};
        case 3: return {false, false, true};
        case 4: return {true, false, true};
        case 5: return {false, true};
        case 6: return {true, true};
        case 7: return {false, true, true};
        case 8: return {true, true, true};
        case 9: return {false, false, false, true};
        case 10: return {true, false, false, true};
        case 11: return {false, false, true, true};
        case 12: return {true, false, true, true};
        case 13: return {false, true, false, true};
        case 14: return {true, true, false, true};
        case 15: return {false, true, true, true};
        case 16: return {true, true, true, true};
    }
    throw std::runtime_error{"io.cpp parse_key_modifiers(): Unknown Mod: " +
                             std::to_string(mod)};
}

auto parse(Control_sequence cs) -> esc::Event
{
    if (cs.final_byte == 'M' || cs.final_byte == 'm')
        return parse_mouse(cs);
    return esc::Key_press{parse_key(cs),
                          parse_key_modifiers(cs.parameter_bytes)};
}

auto parse(Escaped e) -> esc::Event
{
    return esc::Key_press{static_cast<esc::Key>(e.character),
                          {false, false, true}};
}

auto parse(char c) -> esc::Event { return esc::Key_press{esc::char_to_key(c)}; }

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
    std::variant<Initial, Final, Escape, Maybe_escaped, Maybe_CSI, CSI>;

// -----------------------------------------------------------------------------

auto constexpr escape = '\033';

auto next_state(Initial) -> Lexer
{
    if (auto const c = read_char(); c == escape)
        return Escape{};
    else
        return Final{c};
}

auto next_state(Final f) -> Lexer { return f; }

auto next_state(Escape) -> Lexer
{
    if (is_stdin_empty(0))
        return Final{escape};
    else
        return Maybe_escaped{};
}

auto next_state(Maybe_escaped) -> Lexer
{
    auto const c = read_char();
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

auto next_state(CSI state) -> Lexer
{
    if (auto const c = read_char(); c >= 0x40 && c <= 0x7E)
        return Final{Control_sequence{state.value + c}};
    else
        return CSI{state.value + c};
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

namespace esc::io {

auto read() -> esc::Event
{
    auto const token = read_single_token();
    return std::visit([](auto t) { return parse(t); }, token);
}

auto read(int millisecond_timeout) -> std::optional<esc::Event>
{
    if (!is_stdin_empty(millisecond_timeout))
        return esc::io::read();
    return std::nullopt;
}

void disable_canonical_mode_and_echo()
{
    auto tty = termios{};
    tcgetattr(STDIN_FILENO, &tty);
    tty.c_lflag &= ~ICANON;
    tty.c_cc[VMIN] = 1;
    tty.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &tty);
}

void enable_canonical_mode_and_echo()
{
    auto tty = termios{};
    tcgetattr(STDIN_FILENO, &tty);
    tty.c_lflag |= ICANON;
    tty.c_cc[VMIN] = 1;
    tty.c_lflag |= ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &tty);
}

}  // namespace esc::io
