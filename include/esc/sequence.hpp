#pragma once

#include <concepts>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

#include <esc/brush.hpp>
#include <esc/color.hpp>
#include <esc/detail/any_of.hpp>
#include <esc/glyph.hpp>
#include <esc/point.hpp>
#include <esc/trait.hpp>

namespace esc {

// MOVE CURSOR -----------------------------------------------------------------

/**
 * Type alias to move the cursor to a Point.
 */
using Cursor = Point;

/**
 * Tag type to move the cursor to a Point.
 */
// struct CursorPosition {
//    public:
//     constexpr CursorPosition(int x, int y) : at{.x = x, .y = y} {}

//     constexpr CursorPosition(Point p) : at{p} {}

//    public:
//     Point at;
// };

/**
 * Get the control sequence to move the cursor to the specified Point.
 *
 * @details Top-left cell of the terminal is Point{x:0, y:0}. x is horizontal
 *          and y is vertical. The next string output to stdout will happen
 *          starting at \p p.
 * @param   p The Point to move the cursor to.
 * @return  The control sequence to move the cursor to the specified Point.
 */
[[nodiscard]] auto escape(Cursor p) -> std::string;

// CLEAR -----------------------------------------------------------------------

/**
 * Tag type to clear the row the cursor is currently at.
 */
struct BlankRow {};

/**
 * Get the control sequence to clear the row the cursor is currently at.
 *
 * @details Write escape(Cursor) result before to pick which line to clear.
 * @param   BlankRow The tag type to clear the row the cursor is currently at.
 * @return  The control sequence to clear the row the cursor is currently at.
 */
[[nodiscard]] auto escape(BlankRow) -> std::string;

/**
 * Tag type to clear the entire screen.
 */
struct BlankScreen {};

/**
 * Get the control sequence to erase everything on the screen.
 *
 * @param  BlankScreen The tag type to clear the entire screen.
 * @return The control sequence to erase everything on the screen.
 */
[[nodiscard]] auto escape(BlankScreen) -> std::string;

// TRAITS ----------------------------------------------------------------------

/**
 * Get the control sequence to set any number of Traits, clears existing Traits.
 *
 * @details These Traits will be applied to any text written to the screen after
 *          this call, and before another call to escape(Traits). Traits can be
 *          built up into a Traits object with operator|, and can be implicitly
 *          converted into Traits objects. A single Trait::None will clear the
 *          set traits.
 * @param   traits The Traits to set.
 * @return  The control sequence to set any number of Traits, clears existing
 */
[[nodiscard]] auto escape(Traits traits) -> std::string;

/**
 * Overload needed so variadic escape() does not infinite recurse.
 */
[[nodiscard]] auto escape(Trait trait) -> std::string;

/**
 * Get the control sequence to remove all Traits currently set.
 *
 * @details Any text written after will have no Traits.
 * @return The control sequence to remove all Traits currently set.
 */
[[nodiscard]] auto clear_traits() -> std::string;

/**
 * Get the last Traits that were created with escape(Traits).
 *
 * @details May not represent what is on the screen if the last call to
 *          escape(Traits) was not written to stdout.
 * @return  The last Traits that were created with escape(Traits).
 */
[[nodiscard]] auto traits() -> Traits;

// BACKGROUND COLOR ------------------------------------------------------------

/**
 * Get the control sequence to set the background to the specified Color.
 *
 * @param  c The Color to set the background to.
 * @return The control sequence to set the background to the specified Color.
 */
[[nodiscard]] auto escape(ColorBG c) -> std::string;

/**
 * Get the control sequence to set the background to the specified xterm palette
 * index.
 *
 * @param  c The xterm palette index to set the background to.
 * @return The control sequence to set the background to the specified xterm
 *         palette index.
 */
[[nodiscard]] auto escape_bg(ColorIndex c) -> std::string;

/**
 * Get the control sequence to set the background to the specified terminal true
 * color.
 *
 * @param  c The terminal true color to set the background to.
 * @return The control sequence to set the background to the specified terminal
 *         true color.
 */
[[nodiscard]] auto escape_bg(TrueColor c) -> std::string;

/// Return control sequence to reset the background to the terminal default

/**
 * Get the control sequence to reset the background to the terminal default.
 * @return The control sequence to reset the background to the terminal default.
 */
[[nodiscard]] auto escape_bg(DefaultColor c) -> std::string;

/**
 * Get the last Color that was created with escape(ColorBG).
 *
 * @details May not represent what is on the screen if the last call to
 *          escape(ColorBG) was not written to stdout.
 * @return  The last Color that was created with escape(ColorBG).
 */
[[nodiscard]] auto background_color() -> Color;

// FOREGROUND COLOR ------------------------------------------------------------

/**
 * Get the control sequence to set the foreground to the specified Color.
 *
 * @param  c The Color to set the foreground to.
 * @return The control sequence to set the foreground to the specified Color.
 */
[[nodiscard]] auto escape(ColorFG c) -> std::string;

/**
 * Get the control sequence to set the foreground to the specified xterm palette
 * index.
 *
 * @param  c The xterm palette index to set the foreground to.
 * @return The control sequence to set the foreground to the specified xterm
 */
[[nodiscard]] auto escape_fg(ColorIndex c) -> std::string;

/**
 * Get the control sequence to set the foreground to the specified terminal true
 * color.
 *
 * @param  c The terminal true color to set the foreground to.
 * @return The control sequence to set the foreground to the specified terminal
 *         true color.
 */
[[nodiscard]] auto escape_fg(TrueColor c) -> std::string;

/**
 * Get the control sequence to reset the foreground to the terminal default.
 * @return The control sequence to reset the foreground to the terminal default.
 */
[[nodiscard]] auto escape_fg(DefaultColor c) -> std::string;

/**
 * Get the last Color that was created with escape(ColorFG).
 *
 * @details May not represent what is on the screen if the last call to
 *          escape(ColorFG) was not written to stdout.
 * @return  The last Color that was created with escape(ColorFG).
 */
[[nodiscard]] auto foreground_color() -> Color;

// BRUSH -----------------------------------------------------------------------

/**
 * Get the control sequence to set Brush Colors and Traits.
 *
 * @param  b The Brush to set.
 * @return The control sequence to set Brush Colors and Traits.
 */
[[nodiscard]] auto escape(Brush b) -> std::string;

// GLYPH -----------------------------------------------------------------------

/**
 * Get the control sequence to set the Glyph Brush and Symbol.
 *
 * @param  g The Glyph to set.
 * @return The control sequence to set the Glyph Brush and Symbol.
 */
[[nodiscard]] auto escape(Glyph const& g) -> std::string;

/**
 * Get the control sequence to set each Glyph in the GlyphString.
 *
 * @param  gs The GlyphString to set.
 * @return The control sequence to set each Glyph in the GlyphString.
 */
template <GlyphString T>
[[nodiscard]] auto escape(T const& gs) -> std::string
{
    auto result = std::string{};
    for (auto const& g : gs) {
        result += escape(g);
    }
    return result;
}

// CONVENIENCE -----------------------------------------------------------------

/**
 * Types that can have a control sequence generated for them.
 */
template <typename T>
concept Escapable = detail::AnyOf<T,
                                  Cursor,
                                  BlankRow,
                                  BlankScreen,
                                  Trait,
                                  Traits,
                                  ColorBG,
                                  ColorFG,
                                  Brush,
                                  Glyph> ||
                    GlyphString<T>;

/**
 * Convenience function to concatenate multiple escapable objects at once.
 *
 * @details Returns a single string containing the escape sequences for all args
 * @tparam  Args... A list of escapable types.
 * @param   args... A list of escapable objects.
 * @return  A single string containing the escape sequences for all args...
 */
template <Escapable... Args,
          typename = std::enable_if_t<(sizeof...(Args) > 1), void>>
[[nodiscard]] auto escape(Args&&... args) -> std::string
{
    return (escape(std::forward<Args>(args)) + ...);
}

}  // namespace esc