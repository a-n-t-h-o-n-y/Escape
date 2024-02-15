#pragma once

#include <algorithm>
#include <cstdint>
#include <variant>

namespace esc {

// -----------------------------------------------------------------------------

/**
 * Represents a color index for XTerm like palette [0 - 255].
 *
 * @details The color index is a value from 0 to 255, where 0 to 7 are the
 *          classic XTerm system colors, 8 to 15 are the bright versions of the
 *          classic colors, and 16 to 230 are the 216 non-system colors and
 *          231-255 are the grayscale colors. The actual color values are
 *          defined by your terminal.
 * @details This provides the first 16 colors of the XTerm palette by name.
 * @see https://en.wikipedia.org/wiki/ANSI_escape_code#8-bit
 * @see https://www.ditig.com/publications/256-colors-cheat-sheet
 */
struct XColor {
    std::uint16_t value;

    static XColor const Default;
    static XColor const Black;
    static XColor const Red;
    static XColor const Green;
    static XColor const Yellow;
    static XColor const Blue;
    static XColor const Magenta;
    static XColor const Cyan;
    static XColor const White;
    static XColor const BrightBlack;
    static XColor const BrightRed;
    static XColor const BrightGreen;
    static XColor const BrightYellow;
    static XColor const BrightBlue;
    static XColor const BrightMagenta;
    static XColor const BrightCyan;
    static XColor const BrightWhite;

    [[nodiscard]] constexpr bool operator==(XColor const&) const = default;
    [[nodiscard]] constexpr bool operator!=(XColor const&) const = default;
};

// Yes, defining constexpr with const declaration is legal here.
constexpr XColor XColor::Default       = XColor{256};
constexpr XColor XColor::Black         = XColor{0};
constexpr XColor XColor::Red           = XColor{1};
constexpr XColor XColor::Green         = XColor{2};
constexpr XColor XColor::Yellow        = XColor{3};
constexpr XColor XColor::Blue          = XColor{4};
constexpr XColor XColor::Magenta       = XColor{5};
constexpr XColor XColor::Cyan          = XColor{6};
constexpr XColor XColor::White         = XColor{7};
constexpr XColor XColor::BrightBlack   = XColor{8};
constexpr XColor XColor::BrightRed     = XColor{9};
constexpr XColor XColor::BrightGreen   = XColor{10};
constexpr XColor XColor::BrightYellow  = XColor{11};
constexpr XColor XColor::BrightBlue    = XColor{12};
constexpr XColor XColor::BrightMagenta = XColor{13};
constexpr XColor XColor::BrightCyan    = XColor{14};
constexpr XColor XColor::BrightWhite   = XColor{15};

// -----------------------------------------------------------------------------

/**
 * Red, Green, and Blue values; valid range of [0-255] for each color.
 *
 * @details For use with TrueColor.
 */
struct RGB {
   public:
    std::uint8_t red;
    std::uint8_t green;
    std::uint8_t blue;

   public:
    /**
     * Direct construction with red, green and blue values.
     *
     * @param r The red value.
     * @param g The green value.
     * @param b The blue value.
     */
    constexpr RGB(std::uint8_t r, std::uint8_t g, std::uint8_t b)
        : red{r}, green{g}, blue{b}
    {}

    /**
     * Takes a (hex) value and pulls out 8 byte segments for RGB.
     *
     * @details RGB{0x7f9860} 0x7f is red, 0x98 is green, 0x60 is blue.
     * @param hex The hex value to use.
     */
    constexpr RGB(std::uint32_t hex)
        : red{static_cast<std::uint8_t>(hex >> 16 & 0xFF)},
          green{static_cast<std::uint8_t>(hex >> 8 & 0xFF)},
          blue{static_cast<std::uint8_t>(hex & 0xFF)}
    {}
};

/**
 * Hue, Saturation, Lightness values of a color.
 *
 * @details For use with TrueColor.
 * @details Hue is in degrees [0, 359], Saturation is in percent [0, 100],
 *          Lightness is in percent [0, 100].
 */
struct HSL {
    std::uint16_t hue;
    std::uint8_t saturation;
    std::uint8_t lightness;
};

namespace detail {

/**
 * Calculate the absolute value of a number; constexpr.
 * @param x The number to calculate the absolute value of.
 * @return The absolute value of the number.
 */
template <typename T>
auto constexpr abs(T x) -> T
{
    return x < T(0) ? -x : x;
}

/**
 * Calculate the modulo of two numbers; constexpr.
 * @param x The dividend.
 * @param y The divisor.
 * @return The modulo of the two numbers.
 */
template <typename T>
auto constexpr fmod(const T x, const T y) -> T
{
    return (x - T(static_cast<long long>(x / y)) * y);
}

}  // namespace detail

/**
 * Convert HSL to RGB.
 *
 * @param v The HSL value to convert.
 * @return The RGB value.
 */
auto constexpr hsl_to_rgb(HSL v) -> RGB
{
    double const lightness  = v.lightness / 100.;
    double const saturation = v.saturation / 100.;

    auto const c         = (1 - detail::abs((2 * lightness) - 1.)) * saturation;
    double const h_prime = v.hue / 60.;
    double const x = c * (1. - detail::abs(detail::fmod(h_prime, 2.) - 1.));
    double const m = lightness - (c / 2.);

    auto const c_ = static_cast<std::uint8_t>((c + m) * 255);
    auto const x_ = static_cast<std::uint8_t>((x + m) * 255);
    auto const m_ = static_cast<std::uint8_t>(m * 255);

    if (v.hue < 60.) {
        return {c_, x_, m_};
    }
    if (v.hue < 120.) {
        return {x_, c_, m_};
    }
    if (v.hue < 180.) {
        return {m_, c_, x_};
    }
    if (v.hue < 240.) {
        return {m_, x_, c_};
    }
    if (v.hue < 300.) {
        return {x_, m_, c_};
    }
    if (v.hue < 360.) {
        return {c_, m_, x_};
    }
    return {0, 0, 0};
}

/**
 * Convert RGB to HSL.
 *
 * @param v The RGB value to convert.
 * @return The HSL value.
 */
auto constexpr rgb_to_hsl(RGB v) -> HSL
{
    double const r_prime = v.red / 255.;
    double const g_prime = v.green / 255.;
    double const b_prime = v.blue / 255.;

    double const c_max = std::max({r_prime, g_prime, b_prime});
    double const c_min = std::min({r_prime, g_prime, b_prime});
    double const delta = c_max - c_min;

    double const lightness  = (c_max + c_min) / 2.;
    double const saturation = [&] {
        if (delta == 0.) {
            return 0.;
        }
        double const den = 1. - detail::abs(2. * lightness - 1.);
        return delta / den;
    }();
    double const hue = [&] {
        if (delta == 0.) {
            return 0.;
        }
        if (c_max == r_prime) {
            return 60. * detail::fmod((g_prime - b_prime) / delta, 6.);
        }
        if (c_max == g_prime) {
            return 60. * (((b_prime - r_prime) / delta) + 2.);
        }
        if (c_max == b_prime) {
            return 60. * (((r_prime - g_prime) / delta) + 4.);
        }
        return 0.;
    }();
    return {static_cast<std::uint16_t>(hue),
            static_cast<std::uint8_t>(saturation * 100),
            static_cast<std::uint8_t>(lightness * 100)};
}

/**
 * Represents a true color for the terminal display.
 *
 * @details True colors can be used to set an exact color to the terminal
 *          screen via the escape(Color) function.
 */
struct TrueColor {
   public:
    std::uint8_t red;
    std::uint8_t green;
    std::uint8_t blue;

   public:
    /**
     * Construct a true color with RGB values.
     * @param x The RGB value to use.
     */
    constexpr TrueColor(RGB x) : red{x.red}, green{x.green}, blue{x.blue} {}

    /**
     * Construct a true color with HSL values.
     * @param x The HSL value to use.
     */
    constexpr TrueColor(HSL x) : TrueColor{hsl_to_rgb(x)} {}

   public:
    [[nodiscard]] constexpr bool operator==(TrueColor const&) const = default;
    [[nodiscard]] constexpr bool operator!=(TrueColor const&) const = default;
};

/**
 * Alias for TrueColor.
 */
using TColor = TrueColor;

// -----------------------------------------------------------------------------

/**
 * Variant holding any of the color types that can be used.
 *
 * @details escape(Color) will generate an escape sequence for the color.
 */
using Color = std::variant<XColor, TrueColor>;

// -----------------------------------------------------------------------------

/**
 * Represents a foreground color for the terminal display.
 */
struct ColorFG {
    Color value;
};

/**
 * Represents a background color for the terminal display.
 */
struct ColorBG {
    Color value;
};

/**
 * Return a background tag type to use with escape(...) function.
 *
 * @param c The color to use as the background.
 * @return A background tag type to use with escape(...) function.
 */
[[nodiscard]] constexpr auto background(Color c) -> ColorBG { return {c}; }

/**
 * Return a background tag type to use with escape(...) function.
 *
 * @param c The color to use as the background.
 * @return A background tag type to use with escape(...) function.
 */
[[nodiscard]] constexpr auto bg(Color c) -> ColorBG { return {c}; }

/**
 * Return a foreground tag type to use with escape(...) function.
 *
 * @param c The color to use as the foreground.
 * @return A foreground tag type to use with escape(...) function.
 */
[[nodiscard]] constexpr auto foreground(Color c) -> ColorFG { return {c}; }

/**
 * Return a foreground tag type to use with escape(...) function.
 *
 * @param c The color to use as the foreground.
 * @return A foreground tag type to use with escape(...) function.
 */
[[nodiscard]] constexpr auto fg(Color c) -> ColorFG { return {c}; }

}  // namespace esc