#ifndef ESC_SEQUENCE_HPP
#define ESC_SEQUENCE_HPP
#include <cstddef>
#include <string>

#include <esc/brush.hpp>
#include <esc/color.hpp>
#include <esc/color_index.hpp>
#include <esc/default_color.hpp>
#include <esc/detail/is_any_of.hpp>
#include <esc/detail/is_urxvt.hpp>
#include <esc/detail/traits_to_int_sequence.hpp>
#include <esc/point.hpp>
#include <esc/trait.hpp>
#include <esc/true_color.hpp>

namespace esc {

// MOVE CURSOR -----------------------------------------------------------------

/// Tag type to move the cursor to a Point.
struct Cursor_position {
   public:
    constexpr Cursor_position(std::size_t x, std::size_t y) : at{x, y} {}

    constexpr Cursor_position(Point p) : at{p} {}

   public:
    Point at;
};

/// Return control sequence to move the cursor to the specified Point.
/** Top-left cell of the terminal is Point{x:0, y:0}. x is horizontal and y is
 *  vertical. The next string output to stdout will happen starting at \p p. */
[[nodiscard]] inline auto escape(Cursor_position p) -> std::string
{
    return "\033[" + std::to_string(p.at.y + 1) + ';' +
           std::to_string(p.at.x + 1) + 'H';
}

// CLEAR -----------------------------------------------------------------------

/// Tag type to clear a row.
struct Blank_row {};

/// Return control sequence to clear the row the cursor is currently at.
/** Write escape(Cursor_position) result before to pick which line to clear. */
[[nodiscard]] inline auto escape(Blank_row) -> std::string
{
    return "\033["
           "2K";
}

/// Tag type to clear the entire screen.
struct Blank_screen {};

/// Return control sequence to erase everything on the screen.
[[nodiscard]] inline auto escape(Blank_screen) -> std::string
{
    return "\033["
           "?2J";
}

// TRAITS ----------------------------------------------------------------------

namespace detail {
inline auto current_traits = esc::Traits{Trait::None};
}

/// Return control sequence to set any number of Traits, clears existing Traits.
/** These Traits will be applied to any text written to the screen after this
 *  call, and before another call to escape(Traits). Traits can be built up into
 *  a Traits object with operator|, and can be implicitly converted into Traits
 *  objects. A single Trait::None will clear the set traits. */
[[nodiscard]] inline auto escape(Traits traits) -> std::string
{
    detail::current_traits = traits;
    if (traits.data() == 0) {  // Trait::None
        // Clear Traits
        return "\033["
               "22;23;24;25;27;28;29m";
    }
    else {
        return "\033["
               "22;23;24;25;27;28;29;" +
               detail::traits_to_int_sequence(traits) + 'm';
    }
}

/// Overloaded needed so variadic escape() does not inf. recurse.
[[nodiscard]] inline auto escape(Trait trait) -> std::string
{
    return escape(Traits{trait});
}

/// Return control sequence to remove all Traits currently set.
/** Any text written after will have no Traits. */
[[nodiscard]] inline auto clear_traits() -> std::string
{
    detail::current_traits = Traits{};
    return "\033["
           "22;23;24;25;27;28;29m";
}

/// Returns the last Traits that were created with escape(Traits).
/** May not represent what is on the screen if the last call to escape(Traits)
 *  was not written to stdout. */
[[nodiscard]] inline auto traits() -> Traits { return detail::current_traits; }

// COLORS ----------------------------------------------------------------------

namespace detail {
inline auto current_background = esc::Color{esc::Default_color{}};
inline auto current_foreground = esc::Color{esc::Default_color{}};
}  // namespace detail

// BACKGROUND COLOR ------------------------------------------------------------

/// Return control sequence to set background to given xterm palette index.
[[nodiscard]] inline auto escape(BG_Color_index x) -> std::string
{
    detail::current_background = x.value;
    return "\033["
           "48;5;" +
           std::to_string(x.value.value) + 'm';
}

/// Return control sequence to set background to given terminal true color.
[[nodiscard]] inline auto escape(BG_True_color x) -> std::string
{
    detail::current_background = x.value;
    return "\033["
           "48;2;" +
           std::to_string(x.value.red) + ';' + std::to_string(x.value.green) +
           ';' + std::to_string(x.value.blue) + 'm';
}

/// Return control sequence to reset the background to the terminal default
[[nodiscard]] inline auto escape(BG_Default_color) -> std::string
{
    detail::current_background = Default_color{};
    return "\033["
           "49m";
}

/// Returns the last color that was created with set_background().
/** May not represent what is on the screen if the last call to set_background()
 *  was not written to stdout. */
[[nodiscard]] inline auto background_color() -> Color
{
    return detail::current_background;
}

// FOREGROUND COLOR ------------------------------------------------------------

/// Return control sequence to set foreground to given xterm palette index.
[[nodiscard]] inline auto escape(FG_Color_index x) -> std::string
{
    detail::current_foreground = x.value;
    return "\033["
           "38;5;" +
           std::to_string(x.value.value) + 'm';
}

/// Return control sequence to set foreground to given terminal true color.
[[nodiscard]] inline auto escape(FG_True_color x) -> std::string
{
    detail::current_foreground = x.value;
    return "\033["
           "38;2;" +
           std::to_string(x.value.red) + ';' + std::to_string(x.value.green) +
           ';' + std::to_string(x.value.blue) + 'm';
}

/// Return control sequence to reset the foreground to the terminal default
[[nodiscard]] inline auto escape(FG_Default_color) -> std::string
{
    detail::current_foreground = Default_color{};
    return "\033["
           "39m";
}

/// Returns the last color that was created with set_foreground().
/** May not represent what is on the screen if the last call to set_foreground()
 *  was not written to stdout. */
[[nodiscard]] inline auto foreground_color() -> Color
{
    return detail::current_foreground;
}

// BRUSH -----------------------------------------------------------------------

/// Return control sequence to set Brush Colors and Traits.
[[nodiscard]] inline auto escape(Brush b) -> std::string
{
    auto const bg =
        std::visit([](auto c) { return escape(background(c)); }, b.background);
    auto const fg =
        std::visit([](auto c) { return escape(foreground(c)); }, b.foreground);
    return bg + fg + escape(b.traits);
}

// CONVENIENCE -----------------------------------------------------------------

template <typename T>
bool constexpr is_escapable = detail::is_any_of<T,
                                                Cursor_position,
                                                Blank_row,
                                                Blank_screen,
                                                Trait,
                                                Traits,
                                                BG_Color_index,
                                                FG_Color_index,
                                                BG_True_color,
                                                FG_True_color,
                                                BG_Default_color,
                                                FG_Default_color,
                                                Brush>;

/// Convenience function to concatenate multiple escapable objects at once.
/** Returns a single string containing the escape sequences for all args... */
template <typename... Args>
[[nodiscard]] auto escape(Args... args) -> std::string
{
    static_assert(sizeof...(Args) > 0,
                  "escape(...): Must have at least one argument.");
    static_assert((is_escapable<Args> && ...),
                  "escape(...): All Args... must be escapable types.");
    return (escape(args) + ...);
}

}  // namespace esc
#endif  // ESC_SEQUENCE_HPP
