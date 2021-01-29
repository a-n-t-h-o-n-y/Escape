#ifndef ESC_SEQUENCE_HPP
#define ESC_SEQUENCE_HPP
#include <string>

#include <esc/color_index.hpp>
#include <esc/detail/is_urxvt.hpp>
#include <esc/detail/traits_to_int_sequence.hpp>
#include <esc/mask.hpp>
#include <esc/mouse.hpp>
#include <esc/point.hpp>
#include <esc/terminfo.hpp>
#include <esc/trait.hpp>
#include <esc/true_color.hpp>

namespace esc {

/// Set the mouse input mode; determines which Events are generated for mouse.
inline auto set_mouse_mode(Mouse_mode mm) -> std::string
{
    auto const ext_mode = detail::is_urxvt(get_TERM()) ? "1015" : "1006";

    auto result = std::string{"\033[?"};
    switch (mm) {
        case Mouse_mode::Off:
            result.append("1000;10002;1003;").append(ext_mode).append("l");
            break;
        case Mouse_mode::Basic:
            result.append("1000;").append(ext_mode).append("h");
            break;
        case Mouse_mode::Drag:
            result.append("1002;").append(ext_mode).append("h");
            break;
        case Mouse_mode::Move:
            result.append("1003;").append(ext_mode).append("h");
            break;
    }
    return result;
}

/// Return control sequence to enable the alternate screen buffer.
[[nodiscard]] inline auto alternate_screen_buffer() -> std::string
{
    return "\033["
           "?1049h";
}

/// Return control sequence to enable the normal screen buffer.
[[nodiscard]] inline auto normal_screen_buffer() -> std::string
{
    return "\033["
           "?1049l";
}

/// Return control sequence to hide the cursor on the screen.
[[nodiscard]] inline auto hide_cursor() -> std::string
{
    return "\033["
           "?25l";
}

/// Return control sequence to show the cursor on the screen.
[[nodiscard]] inline auto show_cursor() -> std::string
{
    return "\033["
           "?25h";
}

/// Return control sequence to move the cursor to the specified Point.
/** Top-left cell of the terminal is Point{x:0, y:0}. x is horizontal and y is
 *  vertical. The next string output to stdout will happen starting at \p p. */
[[nodiscard]] inline auto move_cursor(Point p) -> std::string
{
    return "\033[" + std::to_string(p.y + 1) + ';' + std::to_string(p.x + 1) +
           'H';
}

namespace detail {

inline auto current_traits = esc::Mask<esc::Trait>{};

}

/// Set any number of Traits at the same time, clears existing set Traits.
/** These Traits will be applied to any text written to the screen after this
 *  call, and before another call to set_traits() or clear_traits(). Traits can
 *  be built up into a Mask<Trait> with operator|, and can be implicitly
 *  converted into Mask<Trait> objects. */
[[nodiscard]] inline auto set_traits(Mask<Trait> traits) -> std::string
{
    detail::current_traits = traits;
    return "\033["
           "22;23;24;25;27;28;29;" +
           detail::traits_to_int_sequence(traits) + 'm';
}

// Remove all Traits currently set, any text written after will have no Traits.
[[nodiscard]] inline auto clear_traits() -> std::string
{
    detail::current_traits = Mask<Trait>{};
    return "\033["
           "22;23;24;25;27;28;29m";
}

/// Returns the last Traits that were created with set_traits().
/** May not represent what is on the screen if the last call to set_traits() was
 *  not written to stdout. */
[[nodiscard]] inline auto get_traits() -> Mask<Trait>
{
    return detail::current_traits;
}

/// Set the background color to the index \p i into the xterm palette.
[[nodiscard]] inline auto set_background(Color_index i) -> std::string
{
    return "\033["
           "48;5;" +
           std::to_string(i.value) + 'm';
}

/// Set the background color to a terminal 'true color'.
[[nodiscard]] inline auto set_background(True_color c) -> std::string
{
    return "\033["
           "48;2;" +
           std::to_string(c.red) + ';' + std::to_string(c.green) + ';' +
           std::to_string(c.blue) + 'm';
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
           "38;5;" +
           std::to_string(i.value) + 'm';
}

/// Set the foreground color to a terminal 'true color'.
[[nodiscard]] inline auto set_foreground(True_color c) -> std::string
{
    return "\033["
           "38;2;" +
           std::to_string(c.red) + ';' + std::to_string(c.green) + ';' +
           std::to_string(c.blue) + 'm';
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
#endif  // ESC_SEQUENCE_HPP
