#ifndef ESC_SEQUENCE_HPP
#define ESC_SEQUENCE_HPP
#include <string>
#include <variant>

#include <esc/brush.hpp>
#include <esc/color.hpp>
#include <esc/color_index.hpp>
#include <esc/default_color.hpp>
#include <esc/detail/is_any_of.hpp>
#include <esc/point.hpp>
#include <esc/trait.hpp>
#include <esc/true_color.hpp>

namespace esc {

// MOVE CURSOR -----------------------------------------------------------------

/// Tag type to move the cursor to a Point.
struct Cursor_position {
   public:
    constexpr Cursor_position(int x, int y) : at{x, y} {}

    constexpr Cursor_position(Point p) : at{p} {}

   public:
    Point at;
};

/// Return control sequence to move the cursor to the specified Point.
/** Top-left cell of the terminal is Point{x:0, y:0}. x is horizontal and y is
 *  vertical. The next string output to stdout will happen starting at \p p. */
[[nodiscard]] auto escape(Cursor_position p) -> std::string;

// CLEAR -----------------------------------------------------------------------

/// Tag type to clear a row.
struct Blank_row {};

/// Return control sequence to clear the row the cursor is currently at.
/** Write escape(Cursor_position) result before to pick which line to clear. */
[[nodiscard]] auto escape(Blank_row) -> std::string;

/// Tag type to clear the entire screen.
struct Blank_screen {};

/// Return control sequence to erase everything on the screen.
[[nodiscard]] auto escape(Blank_screen) -> std::string;

// TRAITS ----------------------------------------------------------------------

/// Return control sequence to set any number of Traits, clears existing Traits.
/** These Traits will be applied to any text written to the screen after this
 *  call, and before another call to escape(Traits). Traits can be built up into
 *  a Traits object with operator|, and can be implicitly converted into Traits
 *  objects. A single Trait::None will clear the set traits. */
[[nodiscard]] auto escape(Traits traits) -> std::string;

/// Overloaded needed so variadic escape() does not inf. recurse.
[[nodiscard]] auto escape(Trait trait) -> std::string;

/// Return control sequence to remove all Traits currently set.
/** Any text written after will have no Traits. */
[[nodiscard]] auto clear_traits() -> std::string;

/// Returns the last Traits that were created with escape(Traits).
/** May not represent what is on the screen if the last call to escape(Traits)
 *  was not written to stdout. */
[[nodiscard]] auto traits() -> Traits;

// BACKGROUND COLOR ------------------------------------------------------------

/// Return control sequence to set background to Color variant.
[[nodiscard]] auto escape(ColorBG c) -> std::string;

/// Return control sequence to set background to given xterm palette index.
[[nodiscard]] auto escape_bg(ColorIndex c) -> std::string;

/// Return control sequence to set background to given terminal true color.
[[nodiscard]] auto escape_bg(TrueColor c) -> std::string;

/// Return control sequence to reset the background to the terminal default
[[nodiscard]] auto escape_bg(DefaultColor c) -> std::string;

/// Returns the last color that was created with set_background().
/** May not represent what is on the screen if the last call to set_background()
 *  was not written to stdout. */
[[nodiscard]] auto background_color() -> Color;

// FOREGROUND COLOR ------------------------------------------------------------

/// Return control sequence to set foreground to Color variant.
[[nodiscard]] auto escape(ColorFG c) -> std::string;

/// Return control sequence to set foreground to given xterm palette index.
[[nodiscard]] auto escape_fg(ColorIndex c) -> std::string;

/// Return control sequence to set foreground to given terminal true color.
[[nodiscard]] auto escape_fg(TrueColor c) -> std::string;

/// Return control sequence to reset the foreground to the terminal default
[[nodiscard]] auto escape_fg(DefaultColor c) -> std::string;

/// Returns the last color that was created with set_foreground().
/** May not represent what is on the screen if the last call to set_foreground()
 *  was not written to stdout. */
[[nodiscard]] auto foreground_color() -> Color;

// BRUSH -----------------------------------------------------------------------

/// Return control sequence to set Brush Colors and Traits.
[[nodiscard]] auto escape(Brush b) -> std::string;

// CONVENIENCE -----------------------------------------------------------------

template <typename T>
bool constexpr is_escapable = detail::is_any_of<T,
                                                Cursor_position,
                                                Blank_row,
                                                Blank_screen,
                                                Trait,
                                                Traits,
                                                ColorBG,
                                                ColorFG,
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
