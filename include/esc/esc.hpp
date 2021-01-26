#ifndef ESC_ESC_HPP
#define ESC_ESC_HPP
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>

#include <esc/color_index.hpp>
#include <esc/point.hpp>
#include <esc/true_color.hpp>

namespace esc {
// CSI "\033[" "\033["
// SS3 "\033O"

[[nodiscard]] inline auto alternate_screen_buffer() -> std::string
{
    return "\033["
           "?1049h";
}

[[nodiscard]] inline auto normal_screen_buffer() -> std::string
{
    return "\033["
           "?1049l";
}

// TODO hide_cursor()
// TODO show_cursor()

[[nodiscard]] inline auto move_cursor(Point p) -> std::string
{
    return "\033[" + std::to_string(p.y + 1) + ";" + std::to_string(p.x + 1) +
           "H";
}

enum class Trait : std::uint8_t {
    Bold             = 1,   // 00000001
    Dim              = 2,   // 00000010
    Italic           = 3,   // 00000011
    Underline        = 4,   // 00000100
    Blink            = 5,   // 00000101
    Inverse          = 7,   // 00000111
    Invisible        = 8,   // 00001000
    Crossed_out      = 9,   // 00001001
    Double_underline = 21,  // 00010101
    Standout         = 0    // 00000000
};

namespace detail {

/// True if all Args... are the same type as T.
template <typename T, typename... Args>
inline bool constexpr are_same = (std::is_same_v<T, Args> && ...);

/// Translates a trait into its numeric string representation, with separators.
[[nodiscard]] inline auto to_string(Trait t) -> std::string
{
    if (t == Trait::Standout)
        return to_string(Trait::Bold) + to_string(Trait::Inverse);
    return std::to_string(static_cast<std::uint8_t>(t)) + ";";
}

}  // namespace detail

/// Set any number of Traits at the same time, clears existing set Traits.
/** These Traits will be applied to any text written to the screen after this
 *  call, and before another call to set_traits(...) or clear_traits(). */
template <typename... Traits,
          typename = std::enable_if_t<(sizeof...(Traits) > 0) &&
                                          detail::are_same<Trait, Traits...>,
                                      void>>
[[nodiscard]] inline auto set_traits(Traits... t) -> std::string
{
    auto traits = (detail::to_string(t) + ...);
    traits.pop_back();  // remove trailing ';'
    return "\033["
           "22;23;24;25;27;28;29;" +
           traits + "m";
}

// Remove all Traits currently set, any text written after will have no Traits.
[[nodiscard]] inline auto clear_traits() -> std::string
{
    return "\033["
           "22;23;24;25;27;28;29m";
}

/// Set the background color to the index \p i into the xterm palette.
[[nodiscard]] inline auto set_background(Color_index i) -> std::string
{
    return "\033["
           "48:5:" +
           std::to_string(i.value) + "m";
}

/// Set the background color to a terminal 'true color'.
[[nodiscard]] inline auto set_background(True_color c) -> std::string
{
    return "\033["
           "48:2::" +
           std::to_string(c.red) + ":" + std::to_string(c.green) + ":" +
           std::to_string(c.blue) + "m";
}

/// Reset the background color to the terminal default.
[[nodiscard]] inline auto reset_background() -> std::string
{
    return "\033["
           "49m";
}

/// Set the foreground color to the index \p i into the xterm palette.
[[nodiscard]] inline auto set_foreground(Color_index i) -> std::string
{
    return "\033["
           "38:5:" +
           std::to_string(i.value) + "m";
}

/// Set the foreground color to a terminal 'true color'.
[[nodiscard]] inline auto set_foreground(True_color c) -> std::string
{
    return "\033["
           "38:2::" +
           std::to_string(c.red) + ":" + std::to_string(c.green) + ":" +
           std::to_string(c.blue) + "m";
}

/// Reset the foreground color to the terminal default.
[[nodiscard]] inline auto reset_foreground() -> std::string
{
    return "\033["
           "39m";
}

/// Reset the foreground and background colors to the terminal default.
[[nodiscard]] inline auto reset_colors() -> std::string
{
    return "\033["
           "39;49m";
}

}  // namespace esc
#endif  // ESC_ESC_HPP
