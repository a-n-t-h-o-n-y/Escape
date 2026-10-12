#include "support/encoding.hpp"

#include <cmath>
#include <stdexcept>

using namespace esc;

static_assert(Color{} == Color{default_color});
static_assert(fg({}) == fg(default_color));
static_assert(bg({}) == bg(default_color));
static_assert(Style{}.foreground == Color{});
static_assert(Style{}.background == Color{});
static_assert((fg(colors::red) | fg(default_color)).foreground == Color{});

TEST(COL_01_defaults_and_palette_names_preserve_color_intent)
{
    CHECK(std::holds_alternative<DefaultColor>(Color{}));
    CHECK(std::holds_alternative<DefaultColor>(ColorFG{}.value));
    CHECK(std::holds_alternative<DefaultColor>(ColorBG{}.value));
    CHECK(std::holds_alternative<IndexedColor>(Color{colors::black}));
    CHECK(Color{RGB{0x000000}} != Color{colors::black});
    CHECK(Color{colors::black} != Color{});
    constexpr IndexedColor named[] = {colors::black,        colors::red,
                                      colors::green,        colors::yellow,
                                      colors::blue,         colors::magenta,
                                      colors::cyan,         colors::white,
                                      colors::bright_black, colors::bright_red,
                                      colors::bright_green, colors::bright_yellow,
                                      colors::bright_blue,  colors::bright_magenta,
                                      colors::bright_cyan,  colors::bright_white};
    for (unsigned i = 0; i < 16; ++i)
        CHECK(named[i] == IndexedColor{static_cast<std::uint8_t>(i)});
    contract::encodes("\x1b[39m\x1b[49m", ColorFG{}, ColorBG{});
}

TEST(COL_01_rgb_and_packed_color_construction_preserve_channels)
{
    CHECK(RGB{0, 127, 255} == RGB{0x007FFF});
    CHECK(RGB{0xF02127} == RGB{RGB{240, 33, 39}});
    CHECK_THROWS(std::invalid_argument, RGB{std::uint32_t{0x1000000}});
    CHECK_THROWS(std::invalid_argument, RGB{std::uint32_t{0xFF000000}});
}

TEST(COL_01_hsl_boundaries_primaries_and_rounding)
{
    CHECK(hsl_to_rgb(HSL{0, 100, 50}) == RGB{255, 0, 0});
    CHECK(hsl_to_rgb(HSL{120, 100, 50}) == RGB{0, 255, 0});
    CHECK(hsl_to_rgb(HSL{240, 100, 50}) == RGB{0, 0, 255});
    CHECK(hsl_to_rgb(HSL{300, 100, 50}) == RGB{255, 0, 255});
    CHECK(hsl_to_rgb(HSL{359, 0, 50}) == RGB{128, 128, 128});
    CHECK(hsl_to_rgb(HSL{0, 100, 0}) == RGB{0, 0, 0});
    CHECK(hsl_to_rgb(HSL{0, 100, 100}) == RGB{255, 255, 255});
    CHECK_THROWS(std::invalid_argument, hsl_to_rgb(HSL{360, 100, 50}));
    CHECK_THROWS(std::invalid_argument, RGB{HSL{0, 101, 50}});
    CHECK_THROWS(std::invalid_argument, RGB{HSL{0, 50, 101}});
}

TEST(COL_01_negative_computed_hues_are_normalized)
{
    auto magenta = rgb_to_hsl(RGB{255, 0, 255});
    CHECK(magenta.hue == 300 && magenta.saturation == 100 && magenta.lightness == 50);
    auto pink = rgb_to_hsl(RGB{255, 0, 128});
    CHECK(pink.hue >= 329 && pink.hue <= 331);
    auto gray = rgb_to_hsl(RGB{127, 127, 127});
    CHECK(gray.hue == 0 && gray.saturation == 0);
}

TEST(COL_01_rgb_hsl_round_trip_uses_tolerance_not_exact_equality)
{
    for (auto rgb :
         {RGB{255, 0, 0}, RGB{3, 100, 200}, RGB{240, 33, 39}, RGB{99, 145, 70}}) {
        auto result = hsl_to_rgb(rgb_to_hsl(rgb));
        CHECK(std::abs(int(result.red) - int(rgb.red)) <= 5);
        CHECK(std::abs(int(result.green) - int(rgb.green)) <= 5);
        CHECK(std::abs(int(result.blue) - int(rgb.blue)) <= 5);
    }
}

TEST(ENC_03_traits_are_canonical_values_with_explicit_mutation)
{
    CHECK(Traits{}.contains(Trait::None));
    CHECK(!Traits{Trait::Bold}.contains(Trait::None));
    CHECK(Traits{Trait::Standout} == (Trait::Bold | Trait::Inverse));
    CHECK(!Traits{Trait::Bold}.contains(Trait::Standout));
    auto original = Traits{Trait::Bold};
    auto combined = original | Trait::Italic;
    CHECK(original == Traits{Trait::Bold});
    CHECK(combined.contains(Trait::Italic));
    CHECK(&original.insert(Trait::Inverse) == &original);
    CHECK(original.contains(Trait::Standout));
    CHECK(&original.remove(Trait::Standout) == &original);
    CHECK(original == Traits{});
    original.insert(Trait::None).remove(Trait::None);
    CHECK(original == Traits{});
    CHECK_THROWS(std::invalid_argument, Traits{static_cast<Trait>(0x8000)});
}

TEST(ENC_03_style_and_color_helpers_have_value_semantics)
{
    CHECK(Style{} == Style{default_color, default_color, Traits{}});
    CHECK(fg(colors::red).value == Color{colors::red});
    CHECK(bg(colors::blue).value == Color{colors::blue});
    auto style = Style{.foreground = colors::red};
    auto changed = style | bg(colors::blue);
    CHECK(style.background == Color{default_color});
    CHECK(changed.background == Color{colors::blue});
    CHECK(changed.foreground == Color{colors::red});
}

TEST(ENC_04_geometry_values_preserve_signed_offsets)
{
    CHECK(Point{} == Point{0, 0});
    CHECK((Point{2, 3} + Point{-1, 4}) == Point{1, 7});
    CHECK((Point{2, 3} - Point{4, 5}) == Point{-2, -2});
    CHECK(Area{} == Area{0, 0});
}
