#pragma once

#include <cstdint>

namespace esc {

/// Color Index for XTerm like palette [0 - 255].
struct ColorIndex {
    std::uint8_t value;

    static ColorIndex const Black;
    static ColorIndex const Red;
    static ColorIndex const Green;
    static ColorIndex const Yellow;
    static ColorIndex const Blue;
    static ColorIndex const Magenta;
    static ColorIndex const Cyan;
    static ColorIndex const White;
    static ColorIndex const BrightBlack;
    static ColorIndex const BrightRed;
    static ColorIndex const BrightGreen;
    static ColorIndex const BrightYellow;
    static ColorIndex const BrightBlue;
    static ColorIndex const BrightMagenta;
    static ColorIndex const BrightCyan;
    static ColorIndex const BrightWhite;
};

// Yes, defining constexpr vs const declaration is legal here.
constexpr ColorIndex ColorIndex::Black         = ColorIndex{0};
constexpr ColorIndex ColorIndex::Red           = ColorIndex{1};
constexpr ColorIndex ColorIndex::Green         = ColorIndex{2};
constexpr ColorIndex ColorIndex::Yellow        = ColorIndex{3};
constexpr ColorIndex ColorIndex::Blue          = ColorIndex{4};
constexpr ColorIndex ColorIndex::Magenta       = ColorIndex{5};
constexpr ColorIndex ColorIndex::Cyan          = ColorIndex{6};
constexpr ColorIndex ColorIndex::White         = ColorIndex{7};
constexpr ColorIndex ColorIndex::BrightBlack   = ColorIndex{8};
constexpr ColorIndex ColorIndex::BrightRed     = ColorIndex{9};
constexpr ColorIndex ColorIndex::BrightGreen   = ColorIndex{10};
constexpr ColorIndex ColorIndex::BrightYellow  = ColorIndex{11};
constexpr ColorIndex ColorIndex::BrightBlue    = ColorIndex{12};
constexpr ColorIndex ColorIndex::BrightMagenta = ColorIndex{13};
constexpr ColorIndex ColorIndex::BrightCyan    = ColorIndex{14};
constexpr ColorIndex ColorIndex::BrightWhite   = ColorIndex{15};

/// Return true if \p a and \p b contain the same index value.
constexpr auto operator==(ColorIndex a, ColorIndex b) -> bool
{
    return a.value == b.value;
}

/// Return false if \p a and \p b contain the same index value.
constexpr auto operator!=(ColorIndex a, ColorIndex b) -> bool
{
    return a.value != b.value;
}

/// Tag type for ColorIndex that is used as a background.
struct ColorIndexBG {
    ColorIndex value;
};

/// Tag type for ColorIndex that is used as a foreground.
struct ColorIndexFG {
    ColorIndex value;
};

/// Return a background tag type to use with escape(...) function.
[[nodiscard]] constexpr auto background(ColorIndex x) -> ColorIndexBG
{
    return {x};
}

/// Return a background tag type to use with escape(...) function.
[[nodiscard]] constexpr auto bg(ColorIndex x) -> ColorIndexBG { return {x}; }

/// Return a foreground tag type to use with escape(...) function.
[[nodiscard]] constexpr auto foreground(ColorIndex x) -> ColorIndexFG
{
    return {x};
}

/// Return a foreground tag type to use with escape(...) function.
[[nodiscard]] constexpr auto fg(ColorIndex x) -> ColorIndexFG { return {x}; }

}  // namespace esc