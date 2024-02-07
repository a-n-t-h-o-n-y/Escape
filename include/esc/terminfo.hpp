#pragma once

#include <cstdint>
#include <string_view>

namespace esc {

/**
 * Return the TERM environment variable from app startup.
 *
 * @details This is the terminal type that the app was started in. This is used
 *          to look up terminal specific settings in the terminfo database.
 * @return  The value of the TERM environment variable.
 */
[[nodiscard]] auto TERM_var() -> std::string_view;

/**
 * Return the number of colors in the terminal's color palette.
 *
 * @details This is the number of possible ColorIndex values that will work with
 *          the current terminal emulator. This is looked up with the TERM env
 *          variable via the terminfo db. It is not always accurate, as some
 *          terminals are capable of displaying more colors than they report;
 *          this should be seen as a lower bound.
 * @return  The number of colors in the terminal's color palette.
 */
[[nodiscard]] auto color_palette_size() -> std::uint16_t;

/**
 * Return true if terminal has support for true colors.
 *
 * @details If true, applications can use the color setting functions with
 *          TrueColor. True color support is determined by the COLORTERM
 *          environment variable.
 * @return  True if the terminal has support for true colors.
 * @see     https://github.com/termstandard/colors
 */
[[nodiscard]] auto has_true_color() -> bool;

}  // namespace esc