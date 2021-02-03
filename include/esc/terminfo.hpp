#ifndef ESC_TERMINFO_HPP
#define ESC_TERMINFO_HPP
#include <cstdint>
#include <string_view>

namespace esc {

/// Return the TERM environment variable from app startup.
[[nodiscard]] auto TERM_var() -> std::string_view;

/// Return the number of colors in the terminal's color palette.
/** This is the number of possible Color_index values that will work with the
 *  current terminal emulator. This is looked up with the TERM env var via the
 *  terminfo db, not always accurate, sometimes more colors can be used. */
[[nodiscard]] auto color_palette_size() -> std::uint16_t;

/// Return true if terminal has support for true colors.
/** If true, applications can use the color setting functions with True_color.
 *  True color support is determined by the COLORTERM environment variable. */
[[nodiscard]] auto has_true_color() -> bool;

}  // namespace esc
#endif  // ESC_TERMINFO_HPP
