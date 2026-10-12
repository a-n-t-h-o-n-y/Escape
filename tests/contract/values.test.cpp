#include "support/encoding.hpp"

#include <cmath>
#include <stdexcept>

using namespace esc;

TEST(COL_01_rgb_and_packed_color_construction_preserve_channels)
{
    CHECK(RGB{0,127,255} == RGB{0x007FFF});
    CHECK(TColor{0xF02127} == TrueColor{RGB{240,33,39}});
    CHECK_THROWS(std::invalid_argument,RGB{std::uint32_t{0x1000000}});
    CHECK_THROWS(std::invalid_argument,TColor{std::uint32_t{0xFF000000}});
}
TEST(COL_01_hsl_boundaries_primaries_and_rounding)
{
    CHECK(hsl_to_rgb(HSL{0,100,50}) == RGB{255,0,0});
    CHECK(hsl_to_rgb(HSL{120,100,50}) == RGB{0,255,0});
    CHECK(hsl_to_rgb(HSL{240,100,50}) == RGB{0,0,255});
    CHECK(hsl_to_rgb(HSL{300,100,50}) == RGB{255,0,255});
    CHECK(hsl_to_rgb(HSL{359,0,50}) == RGB{128,128,128});
    CHECK(hsl_to_rgb(HSL{0,100,0}) == RGB{0,0,0});
    CHECK(hsl_to_rgb(HSL{0,100,100}) == RGB{255,255,255});
    CHECK_THROWS(std::invalid_argument,hsl_to_rgb(HSL{360,100,50}));
    CHECK_THROWS(std::invalid_argument,TrueColor{HSL{0,101,50}});
    CHECK_THROWS(std::invalid_argument,TrueColor{HSL{0,50,101}});
}
TEST(COL_01_negative_computed_hues_are_normalized)
{
    auto magenta=rgb_to_hsl(RGB{255,0,255});
    CHECK(magenta.hue == 300 && magenta.saturation == 100 && magenta.lightness == 50);
    auto pink=rgb_to_hsl(RGB{255,0,128});
    CHECK(pink.hue >= 329 && pink.hue <= 331);
    auto gray=rgb_to_hsl(RGB{127,127,127});
    CHECK(gray.hue == 0 && gray.saturation == 0);
}
TEST(COL_01_rgb_hsl_round_trip_uses_tolerance_not_exact_equality)
{
    for (auto rgb : {RGB{255,0,0},RGB{3,100,200},RGB{240,33,39},RGB{99,145,70}}) {
        auto result=hsl_to_rgb(rgb_to_hsl(rgb));
        CHECK(std::abs(int(result.red)-int(rgb.red)) <= 5);
        CHECK(std::abs(int(result.green)-int(rgb.green)) <= 5);
        CHECK(std::abs(int(result.blue)-int(rgb.blue)) <= 5);
    }
}
TEST(ENC_03_traits_are_canonical_values_with_explicit_mutation)
{
    CHECK(Traits{}.contains(Trait::None));
    CHECK(!Traits{Trait::Bold}.contains(Trait::None));
    CHECK(Traits{Trait::Standout} == (Trait::Bold | Trait::Inverse));
    CHECK(!Traits{Trait::Bold}.contains(Trait::Standout));
    auto original=Traits{Trait::Bold};
    auto combined=original | Trait::Italic;
    CHECK(original == Traits{Trait::Bold});
    CHECK(combined.contains(Trait::Italic));
    CHECK(&original.insert(Trait::Inverse) == &original);
    CHECK(original.contains(Trait::Standout));
    CHECK(&original.remove(Trait::Standout) == &original);
    CHECK(original == Traits{});
    original.insert(Trait::None).remove(Trait::None);
    CHECK(original == Traits{});
    CHECK_THROWS(std::invalid_argument,Traits{static_cast<Trait>(0x8000)});
}
TEST(ENC_03_brush_and_color_helpers_have_value_semantics)
{
    CHECK(Brush{} == Brush{TermColor::Default,TermColor::Default,Traits{}});
    CHECK(fg(XColor::Red).value == foreground(XColor::Red).value);
    CHECK(bg(XColor::Blue).value == background(XColor::Blue).value);
    auto brush=Brush{.foreground=XColor::Red};
    auto changed=brush | bg(XColor::Blue);
    CHECK(brush.background == Color{TermColor::Default});
    CHECK(changed.background == Color{XColor::Blue});
    CHECK(changed.foreground == Color{XColor::Red});
}
TEST(ENC_04_geometry_values_preserve_signed_offsets)
{
    CHECK(Point{} == Point{0,0});
    CHECK((Point{2,3}+Point{-1,4}) == Point{1,7});
    CHECK((Point{2,3}-Point{4,5}) == Point{-2,-2});
    CHECK(Area{} == Area{0,0});
}
