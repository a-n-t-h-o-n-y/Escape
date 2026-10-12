#pragma once

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <variant>

// Windows headers define RGB as a function macro. Preserve the host's macro
// while declaring the value type; brace initialization remains unambiguous.
#ifdef RGB
#    pragma push_macro("RGB")
#    undef RGB
#    define ESC_DETAIL_RESTORE_RGB_MACRO
#endif

namespace esc {

// -------------------------------------------------------------------------------------

/**
 * The default (foreground or background) color set by the terminal application.
 * @details Foreground/Background is determined by the encode(...) function overload set
 * depending on if ColorFG or ColorBG is used.
 */
struct DefaultColor {
    [[nodiscard]] constexpr bool operator==(DefaultColor const&) const = default;
};
inline constexpr DefaultColor default_color{};

// -------------------------------------------------------------------------------------

/**
 * Represents a color index for XTerm like palette.
 * @details The color index is a value from 0 to 255.
 *     [0, 7] ----- Classic XTerm system colors.
 *     [8, 15] ---- 'Bright' versions of the classic colors.
 *     [16, 231] -- 216 non-system colors.
 *     [232, 255] - Grayscale colors.
 * The actual color values are defined by your terminal.
 * @details The colors namespace names the first 16 terminal palette slots.
 * @see https://en.wikipedia.org/wiki/ANSI_escape_code#8-bit
 * @see https://www.ditig.com/publications/256-colors-cheat-sheet
 */
struct IndexedColor {
    std::uint8_t value = 0;
    [[nodiscard]] constexpr bool operator==(IndexedColor const&) const = default;
};

// These select terminal palette slots, preserving the user's configured theme.
namespace colors {
inline constexpr IndexedColor black{0};
inline constexpr IndexedColor red{1};
inline constexpr IndexedColor green{2};
inline constexpr IndexedColor yellow{3};
inline constexpr IndexedColor blue{4};
inline constexpr IndexedColor magenta{5};
inline constexpr IndexedColor cyan{6};
inline constexpr IndexedColor white{7};
inline constexpr IndexedColor bright_black{8};
inline constexpr IndexedColor bright_red{9};
inline constexpr IndexedColor bright_green{10};
inline constexpr IndexedColor bright_yellow{11};
inline constexpr IndexedColor bright_blue{12};
inline constexpr IndexedColor bright_magenta{13};
inline constexpr IndexedColor bright_cyan{14};
inline constexpr IndexedColor bright_white{15};
}  // namespace colors

// -------------------------------------------------------------------------------------

/**
 * A terminal RGB color with byte channels and checked packed-hex/HSL construction.
 */
struct HSL;

struct RGB {
   public:
    std::uint8_t red;
    std::uint8_t green;
    std::uint8_t blue;

   public:
    /**
     * Direct construction with red, green and blue values.
     * @param r The red value.
     * @param g The green value.
     * @param b The blue value.
     */
    constexpr RGB(std::uint8_t r, std::uint8_t g, std::uint8_t b)
        : red{r}, green{g}, blue{b}
    {}

    /**
     * Takes a checked 24-bit value and extracts its three byte channels.
     * @details RGB{0x7f9860} 0x7f is red, 0x98 is green, 0x60 is blue.
     * @param hex The hex value to use.
     */
    explicit constexpr RGB(std::uint32_t hex)
        : red{static_cast<std::uint8_t>(hex >> 16 & 0xFF)},
          green{static_cast<std::uint8_t>(hex >> 8 & 0xFF)},
          blue{static_cast<std::uint8_t>(hex & 0xFF)}
    {
        if (hex > 0xFFFFFF)
            throw std::invalid_argument{"RGB: packed value exceeds 24 bits"};
    }

    explicit constexpr RGB(HSL value);

    [[nodiscard]] constexpr auto operator==(RGB const&) const -> bool = default;
};

/**
 * Hue, Saturation, Lightness values of a color.
 * @details Hue is in degrees [0, 359], Saturation is in percent [0, 100], Lightness is
 * in percent [0, 100].
 */
struct HSL {
    std::uint16_t hue;
    std::uint8_t saturation;
    std::uint8_t lightness;

    [[nodiscard]] constexpr auto operator==(HSL const&) const -> bool = default;
};

namespace detail {

/**
 * Calculate the absolute value of a number; constexpr.
 * @param x The number to calculate the absolute value of.
 * @return The absolute value of the number.
 */
template <typename T>
auto constexpr abs(T x) -> T
{ return x < T(0) ? -x : x; }

/**
 * Calculate the modulo of two numbers; constexpr.
 * @param x The dividend.
 * @param y The divisor.
 * @return The modulo of the two numbers.
 */
template <typename T>
auto constexpr fmod(const T x, const T y) -> T
{ return (x - T(static_cast<long long>(x / y)) * y); }

}  // namespace detail

/**
 * Convert HSL to RGB.
 * @param v The HSL value to convert.
 * @return The RGB value.
 */
auto constexpr hsl_to_rgb(HSL v) -> RGB
{
    if (v.hue >= 360 || v.saturation > 100 || v.lightness > 100)
        throw std::invalid_argument{"HSL: component outside its range"};
    double const lightness = v.lightness / 100.;
    double const saturation = v.saturation / 100.;

    auto const c = (1 - detail::abs((2 * lightness) - 1.)) * saturation;
    double const h_prime = v.hue / 60.;
    double const x = c * (1. - detail::abs(detail::fmod(h_prime, 2.) - 1.));
    double const m = lightness - (c / 2.);

    auto const c_ = static_cast<std::uint8_t>((c + m) * 255 + 0.5);
    auto const x_ = static_cast<std::uint8_t>((x + m) * 255 + 0.5);
    auto const m_ = static_cast<std::uint8_t>(m * 255 + 0.5);

    if (v.hue < 60.) { return {c_, x_, m_}; }
    if (v.hue < 120.) { return {x_, c_, m_}; }
    if (v.hue < 180.) { return {m_, c_, x_}; }
    if (v.hue < 240.) { return {m_, x_, c_}; }
    if (v.hue < 300.) { return {x_, m_, c_}; }
    if (v.hue < 360.) { return {c_, m_, x_}; }
    return {0, 0, 0};
}

/**
 * Convert RGB to HSL.
 * @param v The RGB value to convert.
 * @return The HSL value.
 */
auto constexpr rgb_to_hsl(RGB v) -> HSL
{
    double const r_prime = v.red / 255.;
    double const g_prime = v.green / 255.;
    double const b_prime = v.blue / 255.;

    double const c_max = (std::max)({r_prime, g_prime, b_prime});
    double const c_min = (std::min)({r_prime, g_prime, b_prime});
    double const delta = c_max - c_min;

    double const lightness = (c_max + c_min) / 2.;
    double const saturation = [&] {
        if (delta == 0.) { return 0.; }
        double const den = 1. - detail::abs(2. * lightness - 1.);
        return delta / den;
    }();
    double const hue = [&] {
        if (delta == 0.) { return 0.; }
        if (c_max == r_prime) {
            return 60. * detail::fmod((g_prime - b_prime) / delta, 6.);
        }
        if (c_max == g_prime) { return 60. * (((b_prime - r_prime) / delta) + 2.); }
        if (c_max == b_prime) { return 60. * (((r_prime - g_prime) / delta) + 4.); }
        return 0.;
    }();
    auto const normalized = hue < 0 ? hue + 360. : hue;
    auto const rounded_hue = static_cast<unsigned>(normalized + 0.5) % 360;
    return {static_cast<std::uint16_t>(rounded_hue),
            static_cast<std::uint8_t>(saturation * 100 + 0.5),
            static_cast<std::uint8_t>(lightness * 100 + 0.5)};
}

// -------------------------------------------------------------------------------------

constexpr RGB::RGB(HSL value) : RGB{hsl_to_rgb(value)} {}

/**
 * A terminal default, a palette slot, or an explicit RGB color.
 * Default construction selects the terminal default for the chosen channel.
 * Select a channel with fg(Color) or bg(Color) before encoding.
 */
using Color = std::variant<DefaultColor, IndexedColor, RGB>;

// -------------------------------------------------------------------------------------

/**
 * Represents a foreground color for the terminal display.
 */
struct ColorFG {
    Color value;
    [[nodiscard]] constexpr auto operator==(ColorFG const&) const -> bool = default;
};

/**
 * Represents a background color for the terminal display.
 */
struct ColorBG {
    Color value;
    [[nodiscard]] constexpr auto operator==(ColorBG const&) const -> bool = default;
};

/**
 * Return a background tag type to use with encode(...) function.
 * @param c The color to use as the background.
 * @return A background tag type to use with encode(...) function.
 */
[[nodiscard]] constexpr auto bg(Color c) -> ColorBG { return {c}; }

/**
 * Return a foreground tag type to use with encode(...) function.
 * @param c The color to use as the foreground.
 * @return A foreground tag type to use with encode(...) function.
 */
[[nodiscard]] constexpr auto fg(Color c) -> ColorFG { return {c}; }

}  // namespace esc
#ifdef ESC_DETAIL_RESTORE_RGB_MACRO
#    pragma pop_macro("RGB")
#    undef ESC_DETAIL_RESTORE_RGB_MACRO
#endif
