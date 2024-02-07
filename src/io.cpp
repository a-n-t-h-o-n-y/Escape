#include <esc/io.hpp>

#include <algorithm>
#include <array>
#include <cassert>
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
#include <esc/detail/signals.hpp>
#include <esc/detail/transcode.hpp>
#include <esc/event.hpp>
#include <esc/key.hpp>
#include <esc/terminal.hpp>

#include "detail/tty_file.hpp"

namespace {

using esc::Key;
constexpr auto keymap =
    std::array<esc::Key, 128>{Key::Null,
                              Key::Escape,
                              Key::One,
                              Key::Two,
                              Key::Three,
                              Key::Four,
                              Key::Five,
                              Key::Six,
                              Key::Seven,
                              Key::Eight,
                              Key::Nine,
                              Key::Zero,
                              Key::Minus,
                              Key::Equals,
                              Key::Backspace,
                              Key::Tab,
                              Key::q,
                              Key::w,
                              Key::e,
                              Key::r,
                              Key::t,
                              Key::y,
                              Key::u,
                              Key::i,
                              Key::o,
                              Key::p,
                              Key::Left_bracket,
                              Key::Right_bracket,
                              Key::Enter,
                              Key::LCtrl,
                              Key::a,
                              Key::s,
                              Key::d,
                              Key::f,
                              Key::g,
                              Key::h,
                              Key::j,
                              Key::k,
                              Key::l,
                              Key::Semicolon,
                              Key::Apostrophe,
                              Key::Accent,
                              Key::LShift,
                              Key::Backslash,
                              Key::z,
                              Key::x,
                              Key::c,
                              Key::v,
                              Key::b,
                              Key::n,
                              Key::m,
                              Key::Comma,
                              Key::Period,
                              Key::Forward_slash,
                              Key::RShift,
                              Key::Keypad_asterisk,
                              Key::LAlt,
                              Key::Space,
                              Key::CapsLock,
                              Key::Function1,
                              Key::Function2,
                              Key::Function3,
                              Key::Function4,
                              Key::Function5,
                              Key::Function6,
                              Key::Function7,
                              Key::Function8,
                              Key::Function9,
                              Key::Function10,
                              Key::NumLock,
                              Key::ScrollLock,
                              Key::Keypad7,
                              Key::Keypad8,
                              Key::Keypad9,
                              Key::Keypad_minus,
                              Key::Keypad4,
                              Key::Keypad5,
                              Key::Keypad6,
                              Key::Keypad_plus,
                              Key::Keypad1,
                              Key::Keypad2,
                              Key::Keypad3,
                              Key::Keypad0,
                              Key::Keypad_period,
                              Key::Alt_system_request,  // "magic SysRq key"
                              Key::Null,                // Not commonly used.
                              Key::Null,  // Unlabeled key on non-us keyboards
                              Key::Function11,
                              Key::Function12,
                              Key::Null,  // Random assignments, unused
                              Key::Null,
                              Key::Null,
                              Key::Null,
                              Key::Null,
                              Key::Null,
                              Key::Null,
                              Key::Null,
                              Key::Null,
                              Key::Null,
                              Key::Null,
                              Key::Null,
                              Key::Null,
                              Key::Null,
                              Key::Null,
                              Key::Null,
                              Key::Null,
                              Key::Null,
                              Key::Null,
                              Key::Null,
                              Key::Null,
                              Key::Null,
                              Key::Null,
                              Key::Null,
                              Key::Null,
                              Key::Null,
                              Key::Null,
                              Key::Null,
                              Key::Null,
                              Key::Null,
                              Key::Null,
                              Key::Null,
                              Key::Null,
                              Key::Null,
                              Key::Null,
                              Key::Null,
                              Key::Null,
                              Key::Null,
                              Key::Null};

constexpr auto escape_keymap =
    std::array<esc::Key, 56>{Key::Keypad_enter,
                             Key::RCtrl,
                             Key::Null,
                             Key::Null,
                             Key::Null,
                             Key::Null,
                             Key::Null,
                             Key::Null,
                             Key::Null,
                             Key::Null,
                             Key::Null,
                             Key::Null,
                             Key::Null,
                             Key::Null,
                             Key::Null,
                             Key::LShift_fake,
                             Key::Null,
                             Key::Null,
                             Key::Null,
                             Key::Null,
                             Key::Null,
                             Key::Null,
                             Key::Null,
                             Key::Null,
                             Key::Null,
                             Key::Null,
                             Key::Keypad_forward_slash,
                             Key::RShift_fake,
                             Key::Print_screen_ctrl,
                             Key::RAlt,
                             Key::Null,
                             Key::Null,
                             Key::Null,
                             Key::Null,
                             Key::Null,
                             Key::Null,
                             Key::Null,
                             Key::Null,
                             Key::Null,
                             Key::Null,
                             Key::Null,
                             Key::Null,
                             Key::Null,
                             Key::Ctrl_break,
                             Key::Home_gray,
                             Key::Up_gray,
                             Key::Page_up_gray,
                             Key::Null,
                             Key::Left_gray,
                             Key::Null,
                             Key::Right_gray,
                             Key::End_gray,
                             Key::Down_gray,
                             Key::Page_down_gray,
                             Key::Insert_gray,
                             Key::Delete_gray};

/**
 * Return true if there is nothing to read from file descriptor \p fd.
 *
 * @details Waits up to \p timeout for bytes to be written to the file.
 * @param fd The file descriptor to check for readability.
 * @param millisecond_timeout The maximum time to wait for bytes to be written
 *                            to the file.
 * @return True if there is nothing to read from the file, false if there is
 *         something to read.
 */
[[nodiscard]] auto is_file_readable(int fd, int millisecond_timeout) -> bool
{
    auto file   = pollfd{};
    file.fd     = fd;
    file.events = POLLIN;

    auto const result = poll(&file, 1, millisecond_timeout);

    auto constexpr error   = -1;
    auto constexpr timeout = 0;

    if (result == error) {
        if (errno == EINTR) {  // A signal interrupted poll.
            return true;
        }
        throw std::runtime_error{"io.cpp is_file_readable(): Poll Error"};
    }
    if (result == timeout) {
        return true;
    }
    if (result > 0 && static_cast<bool>(file.revents & POLLIN)) {
        return false;
    }
    throw std::logic_error{"io.cpp is_file_readable(): logic_error."};
}

/**
 * Return true if there is nothing to read from stdin.
 *
 * @details Waits up to \p timeout for bytes to be written to stdin.
 * @param millisecond_timeout The maximum time to wait for bytes to be written
 *                            to stdin.
 * @return True if there is nothing to read from stdin, false if there is
 *         something to read.
 */
[[nodiscard]] auto is_stdin_empty(int millisecond_timeout) -> bool
{
    return is_file_readable(STDIN_FILENO, millisecond_timeout);
}

/**
 * Read a single byte from file descriptor \p fd.
 *
 * @param fd The file descriptor to read from.
 * @return The byte read from \p fd.
 * @throws std::runtime_error if there is an error reading from \p fd.
 */
auto read_byte(int fd) -> unsigned char
{
    unsigned char byte = '\0';
    auto size          = ::read(fd, &byte, 1);
    if (size != 1) {
        throw std::runtime_error{"read_byte(fd): Failed: " +
                                 std::to_string(errno)};
    }
    return byte;
}

/**
 * Read a single byte from file descriptor \p fd, blocking until a byte is read
 * or \p millisecond_timeout passes.
 *
 * @param fd The file descriptor to read from.
 * @param millisecond_timeout The maximum time to wait for a byte to be read.
 * @return The byte read from \p fd, or std::nullopt if the timeout is reached.
 */
auto read_byte(int fd, int millisecond_timeout) -> std::optional<char>
{
    if (!is_stdin_empty(millisecond_timeout)) {
        return read_byte(fd);
    }
    return std::nullopt;
}

// Mouse -----------------------------------------------------------------------

/**
 * Used by urxvt to keep track of the previous mouse button pressed for mouse
 * release events.
 */
auto previous_mouse_btn = esc::Mouse::Button{};

// Token -----------------------------------------------------------------------

/**
 * A control sequence is a sequence of bytes that starts with an escape
 * character and ends with a byte that is not in the range 0x20-0x2F.
 * @details The control sequence is made up of parameter bytes, intermediate
 *          bytes, and a final byte.
 * @see https://en.wikipedia.org/wiki/ANSI_escape_code
 */
class ControlSequence {
   public:
    std::string parameter_bytes;
    std::string intermediate_bytes;
    char final_byte;

   public:
    /**
     * Construct a ControlSequence from a string of bytes.
     * @param sequence The string of bytes to construct the ControlSequence
     *                 from.
     * @throws std::runtime_error if the input is empty.
     */
    explicit ControlSequence(std::string sequence)
    {
        if (sequence.empty()) {
            throw std::runtime_error{"ControlSequence(): Invalid Input"};
        }
        std::ranges::copy_if(sequence, std::back_inserter(parameter_bytes),
                             [](char c) { return c >= '\x30' && c <= '\x3F'; });
        std::ranges::copy_if(sequence, std::back_inserter(intermediate_bytes),
                             [](char c) { return c >= '\x20' && c <= '\x2F'; });
        final_byte = sequence.back();
    }
};

/**
 * Represents an escaped character token.
 */
struct Escaped {
    char character;
};

/**
 * Represents a UTF-8 character token.
 */
struct UTF8 {
    std::array<char, 4> bytes;
};

/**
 * Represents a window resize token.
 */
struct Window {};

/**
 * A Token is one of ControlSequence, Escaped, UTF8, and Window parsed from the
 * terminal's input stream.
 */
using Token = std::variant<ControlSequence, Escaped, UTF8, Window>;

// -----------------------------------------------------------------------------

/// Retrieves the integer type parameter at \p index, returns a type T.
/** \p bytes should only be the parameter bytes. */

/**
 * Retrieves the integer type parameter at \p index, returns a type T.
 * @tparam T The type to return.
 * @tparam index The index of the parameter to retrieve.
 * @param bytes The parameter bytes to retrieve the parameter from.
 * @return The parameter at \p index.
 */
template <typename T, std::size_t index>
auto parameter(std::string bytes) -> T
{
    if constexpr (index != 0) {  // silence gcc warning
        for (auto count = 0uL; count < index; ++count) {
            bytes = bytes.substr(bytes.find(';', 0) + 1);
        }
    }

    return std::stoul(bytes.substr(0, bytes.find(';', 0)));
}

/**
 * Parses a mouse event from a control sequence.
 * @param cs The control sequence to parse the mouse event from.
 * @return The parsed mouse event.
 * @throws std::runtime_error if the mouse event is not parseable.
 */
auto parse_mouse(ControlSequence cs) -> esc::Event
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
        return esc::MouseMove{mouse};

    // Scroll Event
    if (mouse.button == Mouse::Button::ScrollUp ||
        mouse.button == Mouse::Button::ScrollDown) {
        return esc::MouseWheel{mouse};
    }

    // Release Event
    if (cs.final_byte == 'm') {
        return esc::MouseRelease{mouse};
    }

    // urxvt btn release does not mention the button that was released.
    if (!is_sgr && mouse.button == Mouse::Button::None) {
        mouse.button = previous_mouse_btn;
        return esc::MouseRelease{mouse};
    }

    // Press Event
    previous_mouse_btn = mouse.button;
    if (cs.final_byte == 'M') {
        return esc::MousePress{mouse};
    }

    throw std::runtime_error{"io.cpp::parse_mouse(): Bad Mouse Event Parse"};
}

/**
 * Returns the number of parameters in a control sequence string.
 *
 * @details Parameters are delimited by semicolons.
 * @param parameter_bytes The parameter bytes to count.
 * @return The number of parameters in \p parameter_bytes.
 */
auto parameter_count(std::string parameter_bytes) -> std::size_t
{
    return parameter_bytes.empty()
               ? 0
               : std::ranges::count(parameter_bytes, ';') + 1;
}

/**
 * Parses a tilde key from a control sequence.
 * @param parameter_bytes The parameter bytes to parse the tilde key from.
 * @return The parsed tilde key.
 * @throws std::runtime_error if there are no parameter bytes.
 */
auto parse_tilde(std::string parameter_bytes) -> esc::Key
{
    if (parameter_count(parameter_bytes) == 0) {
        throw std::runtime_error{"io.cpp: parse_tilde: No Parameter Bytes"};
    }
    return static_cast<esc::Key>(127 + parameter<int, 0>(parameter_bytes));
}

/**
 * Parses a key from a control sequence.
 * @param cs The control sequence to parse the key from.
 * @return The parsed key.
 * @throws std::runtime_error if the final byte is unknown or there is a parsing
 *         error.
 */
auto parse_key(ControlSequence cs) -> esc::Key
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

/**
 * Parses the key modifiers from a control sequence.
 * @param parameter_bytes The parameter bytes to parse the key modifiers from.
 * @return The parsed key modifiers.
 * @throws std::runtime_error if the parameter bytes are empty.
 */
auto parse_key_modifiers(std::string parameter_bytes) -> esc::Mod
{
    if (parameter_count(parameter_bytes) < 2) {
        return static_cast<esc::Mod>(0);
    }
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

/**
 * Parses a control sequence into an Event.
 * @details This will only produce a mouse or key event.
 * @param cs The control sequence to parse.
 * @return The parsed Event.
 */
auto parse(ControlSequence const& cs) -> esc::Event
{
    if (cs.final_byte == 'M' || cs.final_byte == 'm') {
        return parse_mouse(cs);
    }
    return esc::KeyPress{parse_key(cs) |
                         parse_key_modifiers(cs.parameter_bytes)};
}

/**
 * Parses an escaped character into an Event.
 * @details This will only produce a key press event.
 * @param e The escaped character to parse.
 * @return The parsed Event.
 */
auto parse(Escaped e) -> esc::Event
{
    return esc::KeyPress{static_cast<esc::Key>(e.character)};
}

/**
 * Parses a UTF8 character into an Event.
 * @details This will only produce a key press event.
 * @param x The UTF8 character to parse.
 * @return The parsed Event.
 */
auto parse(UTF8 x) -> esc::Event
{
    return esc::KeyPress{esc::char32_to_key(esc::detail::u8_to_u32(x.bytes))};
}

/**
 * Parses a window resize token into an Event.
 * @param w The window resize token.
 * @return The parsed Event.
 */
auto parse(Window) -> esc::Event { return esc::Resize{esc::terminal_area()}; }

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

/**
 * Used to parse a control sequence from the terminal.
 *
 * @details Creates a state machine to do the parsing with this variant and the
 * next_state function overloads.
 */
using Lexer =
    std::variant<Initial, Final, Escape, Maybe_escaped, Maybe_CSI, CSI, UTF8>;

// -----------------------------------------------------------------------------

auto constexpr escape = '\033';

using esc::detail::window_resize_sig;

auto next_state(Initial) -> Lexer
{
    auto constexpr timeout = 30;  // milliseconds
    while (true) {
        if (window_resize_sig == 1) {
            window_resize_sig = 0;
            return Final{Window{}};
        }
        if (auto const b = read_byte(STDIN_FILENO, timeout); b.has_value()) {
            if (*b == escape) {
                return Escape{};
            }
            else {
                return UTF8{{*b}};
            }
        }
    }
}

auto next_state(Final f) -> Lexer { return f; }

auto next_state(Escape) -> Lexer
{
    if (is_stdin_empty(0)) {
        return Final{UTF8{{escape}}};
    }
    else {
        return Maybe_escaped{};
    }
}

auto next_state(Maybe_escaped) -> Lexer
{
    auto const c = read_byte(STDIN_FILENO);
    if (c != '[' && c != 'O') {
        return Final{Escaped{(char)c}};
    }
    else {
        return Maybe_CSI{(char)c};
    }
}

auto next_state(Maybe_CSI state) -> Lexer
{
    if (is_stdin_empty(0)) {
        return Final{Escaped{state.c}};
    }
    else {
        return CSI{""};
    }
}

auto next_state(CSI const& state) -> Lexer
{
    auto const b = read_byte(STDIN_FILENO);
    if (b >= 0x40 && b <= 0x7E) {
        return Final{ControlSequence{state.value + (char)b}};
    }
    else {
        return CSI{state.value + std::string(1, b)};
    }
}

/**
 * Returns 1 if all set bits in \p mask match up with set bits in value, zeros
 * in mask are treated as wildcards and can be anything in \p value.
 *
 * @param mask The mask to compare with.
 * @param value The value to compare with.
 * @return 1 if all set bits in \p mask match up with set bits in \p value, 0
 *         otherwise.
 */
auto matches_mask(std::uint8_t mask, char value) -> std::uint8_t
{
    return static_cast<std::uint8_t>((mask & value) == mask);
}

/**
 * Returns the number of bytes left to read to form a complete utf8 character.
 * @param first The first byte of the utf8 character.
 * @return The number of bytes left to read to form a complete utf8 character.
 * @details This function is branchless.
 * @see https://en.wikipedia.org/wiki/UTF-8
 */
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
    for (auto count = bytes_left_to_read(state.bytes[0]); count != 0; --count) {
        state.bytes[index++] = read_byte(STDIN_FILENO);
    }
    return Final{state};
}

// -----------------------------------------------------------------------------

/**
 * Read a single input command from the terminal.
 * @details This function will block until a command is read from the terminal.
 * @return The command read from the terminal as a Token.
 */
auto read_single_token() -> Token
{
    auto state = Lexer{Initial{}};
    while (!std::holds_alternative<Final>(state)) {
        state = std::visit([](auto s) { return next_state(s); }, state);
    }
    return std::get<Final>(state).result;
}

// -----------------------------------------------------------------------------

/// Read a single Event from stdin.

/**
 * Read a single Event from stdin.
 *
 * @details This reads a Token from the terminal and then parses it into an
 *          Event.
 * @return The Event read from stdin.
 */
[[nodiscard]] auto do_blocking_read() -> esc::Event
{
    auto const token = read_single_token();
    return std::visit([](auto t) { return parse(t); }, token);
}

/**
 * Check if a byte is a key release or a key press.
 * @param byte The byte to check.
 * @return True if \p byte is a key release, false if it is a key press.
 */
[[nodiscard]] auto is_release(unsigned char byte) -> bool
{
    return byte & 0x80;
}

/**
 * Read and parse a keyboard scancode into a KeyPress or a KeyRelease event.
 * @param fd The file descriptor to read the scancode from.
 * @return The parsed Event.
 * @throws std::runtime_error if the read fails.
 */
[[nodiscard]] auto read_and_parse_scancode(int fd) -> std::optional<esc::Event>
{
    // TODO Cleanup and insert and delete don't quite work.
    // use `sudo showkey -c` to find scancodes.
    // TODO Test on other keyboards.
    auto const byte = read_byte(fd);
    switch (byte) {
        using namespace esc;
        case 0xE0: {
            auto const byte2 = read_byte(fd);
            // Print Screen Press: e0 2a e0 37
            if (byte2 == 0x2A) {
                if (auto const byte3 = read_byte(fd); byte3 == 0xE0) {
                    if (auto const byte4 = read_byte(fd); byte4 == 0x37) {
                        return KeyPress{Key::Print_screen};
                    }
                }
            }
            // Print Screen Release: e0 2a+0x80 e0 37+0x80
            else if ((byte2 - 0x80) == 0x2A) {
                if (auto const byte3 = read_byte(fd); byte3 == 0xE0) {
                    if (auto const byte4 = read_byte(fd) - 0x80;
                        byte4 == 0x37) {
                        return KeyRelease{Key::Print_screen};
                    }
                }
            }

            // Print Screen w/Shift: e0 37
            else if (byte2 == 0x37) {
                return KeyPress{Key::Print_screen_shift};
            }

            // Print Screen w/Shift: e0 37+0x80
            else if ((byte2 & 0x7F) == 0x37) {
                return KeyRelease{Key::Print_screen_shift};
            }

            // Pause w/left or right ctrl: e0 46 e0 c6
            else if (byte2 == 0x46) {
                if (auto const byte3 = read_byte(fd); byte3 == 0xE0) {
                    if (auto const byte4 = read_byte(fd); byte4 == 0xC6) {
                        return KeyPress{Key::Pause_ctrl};
                    }
                }
            }

            switch (byte2) {
                case 0x48: return KeyPress{Key::Arrow_up};
                case 0x48 + 0x80: return KeyRelease{Key::Arrow_up};
                case 0x50: return KeyPress{Key::Arrow_down};
                case 0x50 + 0x80: return KeyRelease{Key::Arrow_down};
                case 0x4D: return KeyPress{Key::Arrow_right};
                case 0x4D + 0x80: return KeyRelease{Key::Arrow_right};
                case 0x4B: return KeyPress{Key::Arrow_left};
                case 0x4B + 0x80: return KeyRelease{Key::Arrow_left};
                default: return std::nullopt;
            }
        }
        case 0xE1: {
            auto const byte2    = read_byte(fd);
            auto const key_byte = byte2 & 0x7F;

            // Pause: e1 1d 45 e1 9d c5
            if (byte2 == 0x1D) {
                if (auto const byte3 = read_byte(fd); byte3 == 0x45) {
                    if (auto const byte4 = read_byte(fd); byte4 == 0xE1) {
                        if (auto const byte5 = read_byte(fd); byte5 == 0x9D) {
                            if (auto const byte6 = read_byte(fd);
                                byte6 == 0xC5) {
                                return KeyPress{Key::Pause};
                            }
                        }
                    }
                }
            }
            if (key_byte > 0x53) {
                return std::nullopt;
            }
            return is_release(byte2)
                       ? Event{KeyRelease{escape_keymap[key_byte - 0x1C]}}
                       : Event{KeyPress{escape_keymap[key_byte - 0x1C]}};
        }

        case 0x54: return KeyPress{Key::Print_screen_alt};
        case 0x54 + 0x80: return KeyRelease{Key::Print_screen_alt};

        case 0x00:  // Keyboard Error
        case 0xAA:  // Basic Assurance Test OK
        case 0xEE:  // Result of echo command
        case 0xF1:  // Reply to command a4:Password not installed
        case 0xFA:  // Acknowledge from kbd
        case 0xFC:  // BAT error or Mouse transmit error
        case 0xFD:  // Internal failure
        case 0xFE:  // Keyboard fails to ack, please resend
        case 0xFF:  // Keyboard erro
            return std::nullopt;

        default:
            return is_release(byte) ? Event{KeyRelease{keymap[byte & 0x7F]}}
                                    : Event{KeyPress{keymap[byte]}};
    }
}

/**
 * Blocks until a read is available for either file descriptor.
 * @param fd_a The first file descriptor to check for readability.
 * @return The file descriptor that is ready, or -1 if timeout or error.
 * @throws std::runtime_error if there is an error polling the file descriptors.
 */
[[nodiscard]] auto timeout_wait_for_reads(int fd_a, int fd_b, int timeout_ms)
    -> int
{
    auto constexpr error   = -1;
    auto constexpr timeout = 0;

    auto files = std::array<pollfd, 2>{{{fd_a, POLLIN, 0}, {fd_b, POLLIN, 0}}};
    auto const result = poll(files.data(), files.size(), timeout_ms);

    if (result == error) {
        if (errno == EINTR) {  // A signal interrupted poll.
            return -1;
        }
        throw std::runtime_error{"timeout_wait_for_reads(): Poll Error"};
    }
    if (result == timeout) {
        return -1;
    }
    if (result > 0 && static_cast<bool>(files[0].revents & POLLIN)) {
        return fd_a;
    }
    if (result > 0 && static_cast<bool>(files[1].revents & POLLIN)) {
        return fd_b;
    }
    throw std::logic_error{"timeout_wait_for_reads(): logic_error."};
}

/**
 * Blocks until a read is available for either file descriptor.
 * @param fd_a The first file descriptor to check for readability.
 * @param fd_b The second file descriptor to check for readability.
 * @return The file descriptor that is ready.
 */
[[nodiscard]] auto blocking_wait_for_reads(int fd_a, int fd_b) -> int
{
    // -1 inf timeout
    return timeout_wait_for_reads(fd_a, fd_b, -1);
}

/**
 * Read an event in alt mode, returns std::nullopt on non-event reads.
 *
 * @details Non-event reads are KeyPress events on stdin, and std::nullopt from
 *          tty.
 * @return The event read from stdin or tty, or std::nullopt if the read was not
 *         an event.
 * @throws std::logic_error if the file descriptor is not stdin or tty.
 */
[[nodiscard]] auto do_maybe_alt_blocking_read() -> std::optional<esc::Event>
{
    auto const file = blocking_wait_for_reads(
        STDIN_FILENO, *esc::detail::tty_file_descriptor);
    if (file == STDIN_FILENO) {
        auto const event = do_blocking_read();
        if (std::holds_alternative<esc::KeyPress>(event)) {
            return std::nullopt;
        }
        else {
            return event;
        }
    }
    else if (file == *esc::detail::tty_file_descriptor) {
        return read_and_parse_scancode(*esc::detail::tty_file_descriptor);
    }
    else {
        throw std::logic_error{"do_maybe_alt_blocking_read(): signal int."};
    }
}

/**
 * Read an event in alt mode, throws out stdin key press events. Reads from tty.
 *
 * @details Waits until an actual event is ready, throwing out non-event reads.
 * @return The event read from stdin or tty.
 */
[[nodiscard]] auto do_alt_blocking_read() -> esc::Event
{
    while (true) {
        auto const file = blocking_wait_for_reads(
            STDIN_FILENO, *esc::detail::tty_file_descriptor);
        if (window_resize_sig == 1 || file == STDIN_FILENO) {
            auto const event = do_blocking_read();
            if (std::holds_alternative<esc::KeyPress>(event)) {
                continue;
            }
            else {
                return event;
            }
        }
        else if (file == *esc::detail::tty_file_descriptor) {
            auto const event =
                read_and_parse_scancode(*esc::detail::tty_file_descriptor);
            if (event.has_value()) {
                return *event;
            }
            else {
                continue;
            }
        }
    }
}

/**
 * Read a single event from stdin with a timeout, returning std::nullopt if no
 * event is read.
 *
 * @param millisecond_timeout The maximum time to wait for an event to be read.
 * @return The event read from stdin, or std::nullopt if no event is read.
 */
[[nodiscard]] auto do_timeout_read(int millisecond_timeout)
    -> std::optional<esc::Event>
{
    if (window_resize_sig == 1 || !is_stdin_empty(millisecond_timeout)) {
        return esc::read();
    }
    return std::nullopt;
}

/// Reads a single token from either STDIN_FILENO or detail::tty_file_descriptor
/** Assumes the tty_file_descriptor is valid. Returns std::nullopt if nothing
 *  was read in the given timeout time. Can return earlier than timeout with
 *  std::nullopt. */

/**
 * Reads a single token from either STDIN_FILENO or detail::tty_file_descriptor.
 * @details Assumes the tty_file_descriptor is valid. Returns std::nullopt if
 *         nothing was read in the given timeout time. Can return earlier than
 *         timeout with std::nullopt.
 * @param millisecond_timeout The maximum time to wait for a token to be read.
 * @return The token read from STDIN_FILENO or detail::tty_file_descriptor, or
 *         std::nullopt if no token is read.
 */
[[nodiscard]] auto do_alt_timeout_read(int millisecond_timeout)
    -> std::optional<esc::Event>
{
    auto const file = timeout_wait_for_reads(
        STDIN_FILENO, *esc::detail::tty_file_descriptor, millisecond_timeout);
    if (file == STDIN_FILENO) {
        auto const result = do_blocking_read();
        if (std::holds_alternative<esc::KeyPress>(result)) {
            return std::nullopt;
        }
        else {
            return result;
        }
    }
    else if (file == *esc::detail::tty_file_descriptor) {
        return do_maybe_alt_blocking_read();
    }
    return std::nullopt;
}

}  // namespace

namespace esc {

void write(char c) { std::putchar(c); }

void write(char32_t c) noexcept(false) { write(esc::detail::u32_to_u8(c)); }

void write(std::string_view sv)
{
    for (auto c : sv) {
        write(c);
    }
}

void write(std::string const& s) { write(s.c_str()); }

void write(char const* s) { std::fputs(s, stdout); }

void write(std::u32string_view sv)
{
    for (auto c : sv) {
        write(c);
    }
}

void flush() { std::fflush(::stdout); }

auto read() -> Event
{
    if (detail::tty_file_descriptor.has_value()) {
        return do_alt_blocking_read();
    }
    else {
        return do_blocking_read();
    }
}

auto read(int millisecond_timeout) -> std::optional<Event>
{
    if (detail::tty_file_descriptor.has_value()) {
        return do_alt_timeout_read(millisecond_timeout);
    }
    else {
        return do_timeout_read(millisecond_timeout);
    }
}

}  // namespace esc
