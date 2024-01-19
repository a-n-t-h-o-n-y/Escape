#ifndef ESC_TRUE_COLOR_HPP
#define ESC_TRUE_COLOR_HPP
#include <algorithm>
#include <cstdint>

namespace esc {

/// Holds Red, Green, and Blue values; valid range of [0-255] for each color.
struct RGB {
    std::uint8_t red;
    std::uint8_t green;
    std::uint8_t blue;

    /// Direct contstruction with red, green and blue values.
    constexpr RGB(std::uint8_t r, std::uint8_t g, std::uint8_t b)
        : red{r}, green{g}, blue{b}
    {}

    /// Takes a (hex) value and pulls out 8 byte segments for RGB.
    /** RGB{0x7f9860} 0x7f is red, 0x98 is green, 0x60 is blue. */
    constexpr RGB(std::uint32_t hex)
        : red{static_cast<std::uint8_t>(hex >> 16 & 0xFF)},
          green{static_cast<std::uint8_t>(hex >> 8 & 0xFF)},
          blue{static_cast<std::uint8_t>(hex & 0xFF)}
    {}
};

/// Holds Hue, Saturation, Lightness values of a color.
struct HSL {
    std::uint16_t hue;        // [0, 359] degrees
    std::uint8_t saturation;  // [0, 100] %
    std::uint8_t lightness;   // [0, 100] %
};

namespace detail {

/// constexpr Absolute Value
template <typename T>
auto constexpr abs(T x) -> T
{
    return x < T(0) ? -x : x;
}

/// constexpr Modulo
template <typename T>
auto constexpr fmod(const T x, const T y) -> T
{
    return (x - T(static_cast<long long>(x / y)) * y);
}

}  // namespace detail

/// Convert HSL to RGB
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

    if (v.hue < 60.)
        return {c_, x_, m_};
    if (v.hue < 120.)
        return {x_, c_, m_};
    if (v.hue < 180.)
        return {m_, c_, x_};
    if (v.hue < 240.)
        return {m_, x_, c_};
    if (v.hue < 300.)
        return {x_, m_, c_};
    if (v.hue < 360.)
        return {c_, m_, x_};
    else
        return {0, 0, 0};
}

/// Convert RGB to HSL
auto constexpr rgb_to_hsl(RGB x) -> HSL
{
    double const r_prime = x.red / 255.;
    double const g_prime = x.green / 255.;
    double const b_prime = x.blue / 255.;

    double const c_max = std::max({r_prime, g_prime, b_prime});
    double const c_min = std::min({r_prime, g_prime, b_prime});
    double const delta = c_max - c_min;

    double const lightness  = (c_max + c_min) / 2.;
    double const saturation = [&] {
        if (delta == 0.)
            return 0.;
        double const den = 1. - detail::abs(2. * lightness - 1.);
        return delta / den;
    }();
    double const hue = [&] {
        if (delta == 0.)
            return 0.;
        if (c_max == r_prime)
            return 60. * detail::fmod((g_prime - b_prime) / delta, 6.);
        if (c_max == g_prime)
            return 60. * (((b_prime - r_prime) / delta) + 2.);
        if (c_max == b_prime)
            return 60. * (((r_prime - g_prime) / delta) + 4.);
        return 0.;
    }();
    return {static_cast<std::uint16_t>(hue),
            static_cast<std::uint8_t>(saturation * 100),
            static_cast<std::uint8_t>(lightness * 100)};
}

/// Holds data for a terminal 'true color'.
/** True colors can be used to set an exact color to the terminal screen. */
class True_color {
   public:
    std::uint8_t red;
    std::uint8_t green;
    std::uint8_t blue;

   public:
    /// Construct a true color with RGB values.
    /** Use this to construct with hex values. */
    constexpr True_color(RGB x) : red{x.red}, green{x.green}, blue{x.blue} {}

    /// Construct a true color with HSL values.
    constexpr True_color(HSL x) : True_color{hsl_to_rgb(x)} {}
};

/// Returns true if the two True_colors are the same.
[[nodiscard]] inline auto operator==(True_color const& lhs,
                                     True_color const& rhs) -> bool
{
    return lhs.red == rhs.red && lhs.green == rhs.green && lhs.blue == rhs.blue;
}

/// Returns true if the two True_colors are not the same.
[[nodiscard]] inline auto operator!=(True_color const& lhs,
                                     True_color const& rhs) -> bool
{
    return !(lhs == rhs);
}

/// Tag type for True_color that is used as a background.
struct BG_True_color {
    True_color value;
};

/// Tag type for True_color that is used as a foreground.
struct FG_True_color {
    True_color value;
};

/// Return a background tag type to use with escape(...) function.
[[nodiscard]] inline auto background(True_color x) -> BG_True_color
{
    return {x};
}

/// Return a foreground tag type to use with escape(...) function.
[[nodiscard]] inline auto foreground(True_color x) -> FG_True_color
{
    return {x};
}

}  // namespace esc
#endif  // ESC_TRUE_COLOR_HPP
