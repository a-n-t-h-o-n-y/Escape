#include <esc/io.hpp>

#include <algorithm>
#include <array>
#include <csignal>
#include <cstdint>
#include <cuchar>
#include <iterator>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>

#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <esc/area.hpp>
#include <esc/debug.hpp>
#include <esc/event.hpp>
#include <esc/key.hpp>

namespace {

/// Return true if there is nothing to ready from stdin.
/** Waits up to \p timeout for bytes to be written to stdin. */
auto is_stdin_empty(int millisecond_timeout) -> bool
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
    if (result > 0 && (file.revents & POLLIN))
        return false;
    throw std::logic_error{"io.cpp is_stdin_empty(): logic_error."};
}

// TODO make a timeout version of this function for use in checking if the
// window has been resized or not.

/// Read a single char from stdin.
auto read_byte() -> char
{
    auto b = char{};
    read(STDIN_FILENO, &b, 1);
    return b;
}

auto read_byte(int millisecond_timeout) -> std::optional<char>
{
    if (!is_stdin_empty(millisecond_timeout))
        return read_byte();
    return std::nullopt;
}

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

struct UTF8 {
    std::array<char, 4> bytes;
};

struct Window {};

using Token = std::variant<Control_sequence, Escaped, UTF8, Window>;

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

auto parse(UTF8 x) -> esc::Event
{
    auto result   = char32_t{};
    auto mb_state = std::mbstate_t{};
    auto error    = std::mbrtoc32(&result, x.bytes.data(), 4, &mb_state);
    if (error == std::size_t(-1))
        throw std::runtime_error{"io.cpp: parse(UTF8): Bad Byte Sequence"};
    return esc::Key_press{esc::char32_to_key(result)};
}

auto parse(Window) -> esc::Event
{
    auto ws           = ::winsize{};
    auto const result = ::ioctl(STDIN_FILENO, TIOCGWINSZ, &ws);
    if (result == -1)
        throw std::runtime_error{"io.cpp parse(Window) Can't Read Window Size"};
    return esc::Window_resize{esc::Area{ws.ws_col, ws.ws_row}};
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

auto next_state(CSI state) -> Lexer
{
    auto const b = read_byte();
    if (b >= 0x40 && b <= 0x7E)
        return Final{Control_sequence{state.value + std::string(1, b)}};
    else
        return CSI{state.value + std::string(1, b)};
}

// Returns 1 if all set bits in \p mask match up with set bits in value, zeros
// in the mask are treated as wildcards and can be anything in \p value.
auto matches_mask(std::uint8_t mask, char value) -> std::uint8_t
{
    return (mask & value) == mask;
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

namespace esc::io {

auto read() -> esc::Event
{
    auto const token = read_single_token();
    return std::visit([](auto t) { return parse(t); }, token);
}

auto read(int millisecond_timeout) -> std::optional<esc::Event>
{
    if (window_resize_sig || !is_stdin_empty(millisecond_timeout))
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

void initialize_stdin()
{
    // TODO should store current termios struct
    if (std::signal(SIGWINCH, &resize_handler) == SIG_ERR)
        throw std::runtime_error{"io.cpp initialize_stdin(): std::signal call"};
    write(enable_mouse());
    disable_canonical_mode_and_echo();
}

}  // namespace esc::io
