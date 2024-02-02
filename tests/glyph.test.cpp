#include <deque>
#include <list>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <esc/glyph.hpp>

using namespace esc;

TEST_CASE("Constructor", "[Glyph]")
{
    SECTION("Default")
    {
        auto const glyph = Glyph{};
        REQUIRE(glyph.symbol == U'\0');
        REQUIRE(glyph.brush == Brush{});
    }

    SECTION("Symbol")
    {
        auto const glyph = Glyph{U'x'};
        REQUIRE(glyph.symbol == U'x');
        REQUIRE(glyph.brush == Brush{});
    }

    SECTION("Symbol, Brush")
    {
        auto const glyph = Glyph{U'x', {.background = ColorIndex::Red}};
        REQUIRE(glyph.symbol == U'x');
        REQUIRE(glyph.brush == Brush{.background = ColorIndex::Red});
    }
}

TEST_CASE(
    "Operator|(Character, ColorBG) works with all character literal types",
    "[Glyph]")
{
    SECTION("char")
    {
        auto const glyph = 'A' | bg(ColorIndex::Red);
        REQUIRE(glyph.symbol == U'A');
    }

    SECTION("signed char")
    {
        auto const glyph = static_cast<signed char>('B') | bg(ColorIndex::Red);
        REQUIRE(glyph.symbol == U'B');
    }

    SECTION("unsigned char")
    {
        auto const glyph =
            static_cast<unsigned char>('C') | bg(ColorIndex::Red);
        REQUIRE(glyph.symbol == U'C');
    }

    SECTION("char8_t")
    {
        auto const glyph = u8'E' | bg(ColorIndex::Red);
        REQUIRE(glyph.symbol == U'E');
    }

    SECTION("char16_t")
    {
        auto const glyph = u'F' | bg(ColorIndex::Red);
        REQUIRE(glyph.symbol == U'F');
    }

    SECTION("char32_t")
    {
        auto const glyph = U'G' | bg(ColorIndex::Red);
        REQUIRE(glyph.symbol == U'G');
    }
}

TEST_CASE(
    "Operator|(Character, ColorFG) works with all character literal types",
    "[Glyph]")
{
    SECTION("char")
    {
        auto const glyph = 'A' | fg(ColorIndex::Red);
        REQUIRE(glyph.symbol == U'A');
    }

    SECTION("signed char")
    {
        auto const glyph = static_cast<signed char>('B') | fg(ColorIndex::Red);
        REQUIRE(glyph.symbol == U'B');
    }

    SECTION("unsigned char")
    {
        auto const glyph =
            static_cast<unsigned char>('C') | fg(ColorIndex::Red);
        REQUIRE(glyph.symbol == U'C');
    }

    SECTION("char8_t")
    {
        auto const glyph = u8'E' | fg(ColorIndex::Red);
        REQUIRE(glyph.symbol == U'E');
    }

    SECTION("char16_t")
    {
        auto const glyph = u'F' | fg(ColorIndex::Red);
        REQUIRE(glyph.symbol == U'F');
    }

    SECTION("char32_t")
    {
        auto const glyph = U'G' | fg(ColorIndex::Red);
        REQUIRE(glyph.symbol == U'G');
    }
}

TEST_CASE("Operator|(Character, Trait) works with all character literal types",
          "[Glyph]")
{
    SECTION("char")
    {
        auto const glyph = 'A' | Trait::Bold;
        REQUIRE(glyph.symbol == U'A');
    }

    SECTION("signed char")
    {
        auto const glyph = static_cast<signed char>('B') | Trait::Bold;
        REQUIRE(glyph.symbol == U'B');
    }

    SECTION("unsigned char")
    {
        auto const glyph = static_cast<unsigned char>('C') | Trait::Bold;
        REQUIRE(glyph.symbol == U'C');
    }

    SECTION("char8_t")
    {
        auto const glyph = u8'E' | Trait::Bold;
        REQUIRE(glyph.symbol == U'E');
    }

    SECTION("char16_t")
    {
        auto const glyph = u'F' | Trait::Bold;
        REQUIRE(glyph.symbol == U'F');
    }

    SECTION("char32_t")
    {
        auto const glyph = U'G' | Trait::Bold;
        REQUIRE(glyph.symbol == U'G');
    }
}

template <typename Container>
auto test_glyphstring_pipe_ops_lval() -> void
{
    auto glyphs = Container(5, {U'a', {.traits = Trait::Bold}});

    SECTION("ColorBG")
    {
        glyphs | bg(ColorIndex::Green);

        for (auto const& glyph : glyphs) {
            REQUIRE(glyph == Glyph{U'a',
                                   {.background = ColorIndex::Green,
                                    .traits     = Trait::Bold}});
        }
    }

    SECTION("ColorFG")
    {
        glyphs | fg(ColorIndex::Green);

        for (auto const& glyph : glyphs) {
            REQUIRE(glyph == Glyph{U'a',
                                   {.foreground = ColorIndex::Green,
                                    .traits     = Trait::Bold}});
        }
    }

    SECTION("Trait")
    {
        glyphs | Trait::Italic;

        for (auto const& glyph : glyphs) {
            REQUIRE(glyph ==
                    Glyph{U'a', {.traits = Trait::Bold | Trait::Italic}});
        }
    }

    SECTION("RemoveTrait")
    {
        glyphs | Trait::Bold | remove_trait(Trait::Bold);

        for (auto const& glyph : glyphs) {
            REQUIRE(glyph == Glyph{U'a'});
        }
    }

    SECTION("ColorBG, ColorFG, Trait")
    {
        glyphs | bg(ColorIndex::Green) | fg(ColorIndex::Red) | Trait::Dim;

        for (auto const& glyph : glyphs) {
            REQUIRE(glyph == Glyph{U'a',
                                   {.background = ColorIndex::Green,
                                    .foreground = ColorIndex::Red,
                                    .traits     = Trait::Bold | Trait::Dim}});
        }
    }
}

template <typename Container>
auto test_glyphstring_pipe_ops_rval() -> void
{
    SECTION("ColorBG")
    {
        auto const glyphs = Container(5, {U'a', {.traits = Trait::Bold}}) |
                            bg(ColorIndex::Green);

        for (auto const& glyph : glyphs) {
            REQUIRE(glyph == Glyph{U'a',
                                   {.background = ColorIndex::Green,
                                    .traits     = Trait::Bold}});
        }
    }

    SECTION("ColorFG")
    {
        auto const glyphs = Container(5, {U'a', {.traits = Trait::Bold}}) |
                            fg(ColorIndex::Green);

        for (auto const& glyph : glyphs) {
            REQUIRE(glyph == Glyph{U'a',
                                   {.foreground = ColorIndex::Green,
                                    .traits     = Trait::Bold}});
        }
    }

    SECTION("Trait")
    {
        auto const glyphs =
            Container(5, {U'a', {.traits = Trait::Bold}}) | Trait::Italic;

        for (auto const& glyph : glyphs) {
            REQUIRE(glyph ==
                    Glyph{U'a', {.traits = Trait::Bold | Trait::Italic}});
        }
    }

    SECTION("RemoveTrait")
    {
        auto const glyphs = Container(5, {U'a', {.traits = Trait::Bold}}) |
                            Trait::Bold | remove_trait(Trait::Bold);

        for (auto const& glyph : glyphs) {
            REQUIRE(glyph == Glyph{U'a'});
        }
    }

    SECTION("ColorBG, ColorFG, Trait")
    {
        auto const glyphs = Container(5, {U'a', {.traits = Trait::Bold}}) |
                            bg(ColorIndex::Green) | fg(ColorIndex::Red) |
                            Trait::Dim;

        for (auto const& glyph : glyphs) {
            REQUIRE(glyph == Glyph{U'a',
                                   {.background = ColorIndex::Green,
                                    .foreground = ColorIndex::Red,
                                    .traits     = Trait::Bold | Trait::Dim}});
        }
    }
}

TEST_CASE("Operator|(GlyphString, ...) where GlyphString is a std::vector",
          "[GlyphString]")
{
    test_glyphstring_pipe_ops_lval<std::vector<Glyph>>();
    test_glyphstring_pipe_ops_rval<std::vector<Glyph>>();
}

TEST_CASE("Operator|(GlyphString, ...) where GlyphString is a std::list",
          "[GlyphString]")
{
    test_glyphstring_pipe_ops_lval<std::list<Glyph>>();
    test_glyphstring_pipe_ops_rval<std::list<Glyph>>();
}

TEST_CASE("Operator|(GlyphString, ...) where GlyphString is a std::deque",
          "[GlyphString]")
{
    test_glyphstring_pipe_ops_lval<std::deque<Glyph>>();
    test_glyphstring_pipe_ops_rval<std::deque<Glyph>>();
}

template <typename StringType>
auto test_string_pipe_ops(StringType abc) -> void
{
    SECTION("ColorBG")
    {
        auto const gs = abc | bg(ColorIndex::Green);
        REQUIRE(gs.size() == 3);
        REQUIRE(gs[0] == Glyph{U'a', {.background = ColorIndex::Green}});
        REQUIRE(gs[1] == Glyph{U'b', {.background = ColorIndex::Green}});
        REQUIRE(gs[2] == Glyph{U'c', {.background = ColorIndex::Green}});
    }

    SECTION("ColorFG")
    {
        auto const gs = abc | fg(ColorIndex::Green);
        REQUIRE(gs.size() == 3);
        REQUIRE(gs[0] == Glyph{U'a', {.foreground = ColorIndex::Green}});
        REQUIRE(gs[1] == Glyph{U'b', {.foreground = ColorIndex::Green}});
        REQUIRE(gs[2] == Glyph{U'c', {.foreground = ColorIndex::Green}});
    }

    SECTION("Trait")
    {
        auto const gs = abc | Trait::Bold;
        REQUIRE(gs.size() == 3);
        REQUIRE(gs[0] == Glyph{U'a', {.traits = Trait::Bold}});
        REQUIRE(gs[1] == Glyph{U'b', {.traits = Trait::Bold}});
        REQUIRE(gs[2] == Glyph{U'c', {.traits = Trait::Bold}});
    }

    SECTION("RemoveTrait")
    {
        auto const gs = abc | Trait::Bold | remove_trait(Trait::Bold);
        REQUIRE(gs.size() == 3);
        REQUIRE(gs[0] == Glyph{U'a'});
        REQUIRE(gs[1] == Glyph{U'b'});
        REQUIRE(gs[2] == Glyph{U'c'});
    }

    SECTION("ColorBG, ColorFG, Trait")
    {
        auto const gs =
            abc | bg(ColorIndex::Green) | fg(ColorIndex::Red) | Trait::Dim;
        REQUIRE(gs.size() == 3);
        auto const brush = Brush{.background = ColorIndex::Green,
                                 .foreground = ColorIndex::Red,
                                 .traits     = Trait::Dim};
        REQUIRE(gs[0] == Glyph{U'a', brush});
        REQUIRE(gs[1] == Glyph{U'b', brush});
        REQUIRE(gs[2] == Glyph{U'c', brush});
    }
}

TEST_CASE("Operator|(StringType ...)", "[GlyphString]")
{
    SECTION("char32_t const*") { test_string_pipe_ops(U"abc"); }

    SECTION("std::u32string") { test_string_pipe_ops(std::u32string{U"abc"}); }

    SECTION("char const*") { test_string_pipe_ops("abc"); }

    SECTION("std::string") { test_string_pipe_ops(std::string{"abc"}); }
}

TEST_CASE("Operator|(StringType) with multibyte symbols", "[GlyphString]")
{
    SECTION("char32_t const*")
    {
        auto const gs = U"🚦" | bg(ColorIndex::Green);
        REQUIRE(gs.size() == 1);
        REQUIRE(gs[0] == Glyph{U'🚦', {.background = ColorIndex::Green}});
    }

    SECTION("Empty char32_t const*")
    {
        auto const gs = U"" | bg(ColorIndex::Green);
        REQUIRE(gs.empty());
    }

    SECTION("std::u32string_view")
    {
        auto const gs =
            std::u32string_view{U"A¢€😀 世界𐍈"} | bg(ColorIndex::Green);

        REQUIRE(gs.size() == 8);

        REQUIRE(gs[1] == Glyph{U'¢', {.background = ColorIndex::Green}});
        REQUIRE(gs[2] == Glyph{U'€', {.background = ColorIndex::Green}});
        REQUIRE(gs[3] == Glyph{U'😀', {.background = ColorIndex::Green}});
        REQUIRE(gs[4] == Glyph{U' ', {.background = ColorIndex::Green}});
        REQUIRE(gs[5] == Glyph{U'世', {.background = ColorIndex::Green}});
        REQUIRE(gs[6] == Glyph{U'界', {.background = ColorIndex::Green}});
        REQUIRE(gs[7] == Glyph{U'𐍈', {.background = ColorIndex::Green}});
    }

    SECTION("char const*")
    {
        auto const gs = "🚦" | bg(ColorIndex::Green);
        REQUIRE(gs.size() == 1);
        REQUIRE(gs[0] == Glyph{U'🚦', {.background = ColorIndex::Green}});
    }

    SECTION("Empty char const*")
    {
        auto const gs = "" | bg(ColorIndex::Green);
        REQUIRE(gs.empty());
    }

    SECTION("std::string_view")
    {
        auto const gs = std::string_view{"A¢€😀 世界𐍈"} | bg(ColorIndex::Green);

        REQUIRE(gs.size() == 8);

        REQUIRE(gs[1] == Glyph{U'¢', {.background = ColorIndex::Green}});
        REQUIRE(gs[2] == Glyph{U'€', {.background = ColorIndex::Green}});
        REQUIRE(gs[3] == Glyph{U'😀', {.background = ColorIndex::Green}});
        REQUIRE(gs[4] == Glyph{U' ', {.background = ColorIndex::Green}});
        REQUIRE(gs[5] == Glyph{U'世', {.background = ColorIndex::Green}});
        REQUIRE(gs[6] == Glyph{U'界', {.background = ColorIndex::Green}});
        REQUIRE(gs[7] == Glyph{U'𐍈', {.background = ColorIndex::Green}});
    }
}