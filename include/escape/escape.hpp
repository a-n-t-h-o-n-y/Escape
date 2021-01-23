#ifndef ESCAPE_ESCAPE_HPP
#define ESCAPE_ESCAPE_HPP
#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>

// OUTPUT
// move cursor to x,y index
// clear screen
// reset colors and traits to defaults
// save terminal state and reset to it
// set the foreground and background colors
// set traits, remove traits, clear traits

// INPUT
// get keyboard input
// get mouse input
// get terminal dimensions

// TODO move to some sort of utility/common directory in its own file.
template <std::size_t N>
class Static_string {
   public:
    constexpr explicit Static_string(char const (&lit)[N + 1]) : chars_{}
    {
        for (auto i = 0uL; i < N; ++i)
            chars_[i] = lit[i];
    }

   private:
    template <std::size_t N1, typename T = std::enable_if_t<(N1 < N), void>>
    constexpr Static_string(Static_string<N1> const& a,
                            Static_string<N - N1> const& b)
        : chars_{}
    {
        for (auto i = 0uL; i < N1; ++i)
            chars_[i] = a[i];
        for (auto i = 0uL; i < N - N1; ++i)
            chars_[N1 + i] = b[i];
    }

   public:
    auto constexpr length() const -> std::size_t { return N; }

    template <std::size_t index>
    auto constexpr get() const -> std::size_t
    {
        static_assert(index < N);
        return chars_[index];
    }

    auto constexpr operator[](std::size_t index) const -> char
    {
        return chars_[index];
    }

   private:
    std::array<char, N + 1> chars_;

    template <std::size_t N1, std::size_t N2>
    friend auto constexpr operator+(Static_string<N1> const&,
                                    Static_string<N2> const&)
        -> Static_string<N1 + N2>;
};

// Deducation Guide
template <std::size_t NPlus1>
Static_string(char const (&lit)[NPlus1]) -> Static_string<NPlus1 - 1>;

template <std::size_t N1, std::size_t N2>
auto constexpr operator+(Static_string<N1> const& a, Static_string<N2> const& b)
    -> Static_string<N1 + N2>
{
    return Static_string<N1 + N2>{a, b};
}

namespace esc {

inline auto ESC() -> std::string { return "\033"; }

inline auto CSI() -> std::string { return ESC() + "["; }

inline auto enter_alt_buffer_mode() -> std::string { return "\033[?1049h"; }

inline auto exit_alt_buffer_mode() -> std::string { return "\033[?1049l"; }

inline auto save_screen() -> std::string { return "\0337"; }

/// Restores the screen to the state at the last call to save_screen.
/** Does not clear anything written to the screen.. */
inline auto restore_screen() -> std::string { return "\0338"; }

inline auto reset_terminal() -> std::string { return "\033c"; }

inline auto clear_screen() -> std::string { return "\033[2J"; }

// These don't do what you think they do, not sure what it is they do.
// Instead you'll need to use OS specific functionality, it is fine if this is
// linux specific library, might be easy enough to add an implementations for
// windows/macOS later to stop echo of input.
// use the tcsetattr function from termios.h header.
inline auto keyboard_autorepeat_on() -> std::string { return "\033[?8h"; }
inline auto keyboard_autorepeat_off() -> std::string { return "\033[?8l"; }

inline auto reset_colors_and_traits() -> std::string { return "\033[0m"; }

inline auto home_cursor() -> std::string { return "\033[H"; }  // rename

inline auto move_cursor(std::size_t line, std::size_t column)
{
    return "\033[" + std::to_string(line) + ';' + std::to_string(column) + 'H';
}

/// Set foreground color to \p id [0 - 255]
inline auto foreground(std::size_t id) -> std::string
{
    return "\033[38;5;" + std::to_string(id) + "m";
}

/// Set background color to \p id [0 - 255]
inline auto background(std::size_t id) -> std::string
{
    return "\033[48;5;" + std::to_string(id) + "m";
}

inline auto set_colors(std::size_t fg, std::size_t bg) -> std::string
{
    return "\033[38;5;" + std::to_string(fg) + ";48;5;" + std::to_string(bg) +
           "m";
}

namespace ext {
/// extension from xterm; window title
inline auto window_title(std::string const& title) -> std::string
{
    return "\x1b]0;" + title + "\x07";
}
// mouse, etc..
// make sure to say where the extension is from in the comment.

}  // namespace ext

}  // namespace esc
#endif  // ESCAPE_ESCAPE_HPP
