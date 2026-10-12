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

/// The terminal theme's default color. Select a channel with fg() or bg().
struct DefaultColor {
    [[nodiscard]] constexpr bool operator==(DefaultColor const&) const = default;
};

inline constexpr DefaultColor default_color{};

/// A terminal palette slot: 0–15 are theme colors, 16–231 the color cube,
/// 232–255 grayscale. The terminal owns the actual palette.
struct IndexedColor {
    std::uint8_t value = 0;
    [[nodiscard]] constexpr bool operator==(IndexedColor const&) const = default;
};

/// These select terminal palette slots, preserving the user's configured theme.
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

struct HSL;

/// Explicit 24-bit color, independent of the terminal's palette.
struct RGB {
   public:
    std::uint8_t red;
    std::uint8_t green;
    std::uint8_t blue;

   public:
    /// Construct from three byte channels.
    constexpr RGB(std::uint8_t r, std::uint8_t g, std::uint8_t b)
        : red{r}, green{g}, blue{b}
    {}

    /// Construct from 0xRRGGBB; values above 0xFFFFFF throw invalid_argument.
    explicit constexpr RGB(std::uint32_t hex)
        : red{static_cast<std::uint8_t>(hex >> 16 & 0xFF)},
          green{static_cast<std::uint8_t>(hex >> 8 & 0xFF)},
          blue{static_cast<std::uint8_t>(hex & 0xFF)}
    {
        if (hex > 0xFFFFFF)
            throw std::invalid_argument{"RGB: packed value exceeds 24 bits"};
    }

    /// Convert validated HSL to byte channels.
    explicit constexpr RGB(HSL value);

    [[nodiscard]] constexpr auto operator==(RGB const&) const -> bool = default;
};

/// Hue in degrees [0,359], saturation and lightness in percent [0,100].
/// Conversion validates the components.
struct HSL {
    std::uint16_t hue;
    std::uint8_t saturation;
    std::uint8_t lightness;

    [[nodiscard]] constexpr auto operator==(HSL const&) const -> bool = default;
};

namespace detail {

/// Constexpr absolute value.
template <typename T>
auto constexpr abs(T x) -> T
{ return x < T(0) ? -x : x; }

/// Constexpr floating-point remainder.
template <typename T>
auto constexpr fmod(const T x, const T y) -> T
{ return (x - T(static_cast<long long>(x / y)) * y); }

}  // namespace detail

/// Convert valid HSL to RGB, rounding channels to the nearest byte.
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

/// Convert RGB to HSL, rounding hue and percentages to whole numbers.
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

constexpr RGB::RGB(HSL value) : RGB{hsl_to_rgb(value)} {}

/**
 * A terminal default, a palette slot, or an explicit RGB color.
 * Default construction selects the terminal default for the chosen channel.
 * Select a channel with fg(Color) or bg(Color) before encoding.
 */
using Color = std::variant<DefaultColor, IndexedColor, RGB>;

/// A foreground color attribute.
struct ColorFG {
    Color value;
    [[nodiscard]] constexpr auto operator==(ColorFG const&) const -> bool = default;
};

/// A background color attribute.
struct ColorBG {
    Color value;
    [[nodiscard]] constexpr auto operator==(ColorBG const&) const -> bool = default;
};

/// Select the background channel for this color.
[[nodiscard]] constexpr auto bg(Color c) -> ColorBG { return {c}; }

/// Select the foreground channel for this color.
[[nodiscard]] constexpr auto fg(Color c) -> ColorFG { return {c}; }

}  // namespace esc
#ifdef ESC_DETAIL_RESTORE_RGB_MACRO
#    pragma pop_macro("RGB")
#    undef ESC_DETAIL_RESTORE_RGB_MACRO
#endif
