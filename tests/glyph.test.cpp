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

TEST_CASE("Chained pipe ops")
{
    SECTION("Glyph")
    {
        auto const g = Glyph{U'a'} | bg(ColorIndex::Green) |
                       fg(ColorIndex::Red) | Trait::Bold | Trait::Italic |
                       remove_trait(Trait::Bold);
        REQUIRE(g == Glyph{.symbol = U'a',
                           .brush  = {.background = ColorIndex::Green,
                                      .foreground = ColorIndex::Red,
                                      .traits     = Trait::Italic}});
    }

    SECTION("GlyphString")
    {
        auto const gs = "abc" | bg(ColorIndex::Green) | fg(ColorIndex::Red) |
                        Trait::Bold | Trait::Italic | remove_trait(Trait::Bold);

        REQUIRE(gs.size() == 3);
        REQUIRE(gs[0] == Glyph{.symbol = U'a',
                               .brush  = {.background = ColorIndex::Green,
                                          .foreground = ColorIndex::Red,
                                          .traits     = Trait::Italic}});
        REQUIRE(gs[1] == Glyph{.symbol = U'b',
                               .brush  = {.background = ColorIndex::Green,
                                          .foreground = ColorIndex::Red,
                                          .traits     = Trait::Italic}});
        REQUIRE(gs[2] == Glyph{.symbol = U'c',
                               .brush  = {.background = ColorIndex::Green,
                                          .foreground = ColorIndex::Red,
                                          .traits     = Trait::Italic}});
    }
}

TEST_CASE("operator+=(GlyphString, ...)", "[Glyph]")
{
    SECTION("GS += GS")
    {
        auto gs = "abc" | bg(ColorIndex::Green);
        gs += "def" | bg(ColorIndex::Red);

        REQUIRE(gs.size() == 6);
        REQUIRE(gs[0] == Glyph{U'a', {.background = ColorIndex::Green}});
        REQUIRE(gs[1] == Glyph{U'b', {.background = ColorIndex::Green}});
        REQUIRE(gs[2] == Glyph{U'c', {.background = ColorIndex::Green}});
        REQUIRE(gs[3] == Glyph{U'd', {.background = ColorIndex::Red}});
        REQUIRE(gs[4] == Glyph{U'e', {.background = ColorIndex::Red}});
        REQUIRE(gs[5] == Glyph{U'f', {.background = ColorIndex::Red}});
    }

    SECTION("GS += std::u32string_view")
    {
        auto gs = "abc" | bg(ColorIndex::Green);
        gs += U"d😀f";

        REQUIRE(gs.size() == 6);
        REQUIRE(gs[0] == Glyph{U'a', {.background = ColorIndex::Green}});
        REQUIRE(gs[1] == Glyph{U'b', {.background = ColorIndex::Green}});
        REQUIRE(gs[2] == Glyph{U'c', {.background = ColorIndex::Green}});
        REQUIRE(gs[3] == Glyph{U'd'});
        REQUIRE(gs[4] == Glyph{U'😀'});
        REQUIRE(gs[5] == Glyph{U'f'});
    }

    SECTION("GS += std::string_view")
    {
        auto gs = "abc" | bg(ColorIndex::Green);
        gs += "d😀f";

        REQUIRE(gs.size() == 6);
        REQUIRE(gs[0] == Glyph{U'a', {.background = ColorIndex::Green}});
        REQUIRE(gs[1] == Glyph{U'b', {.background = ColorIndex::Green}});
        REQUIRE(gs[2] == Glyph{U'c', {.background = ColorIndex::Green}});
        REQUIRE(gs[3] == Glyph{U'd'});
        REQUIRE(gs[4] == Glyph{U'😀'});
        REQUIRE(gs[5] == Glyph{U'f'});
    }

    SECTION("GS += Glyph")
    {
        auto gs = "abc" | bg(ColorIndex::Green);
        gs += Glyph{U'd', {.background = ColorIndex::Red}};

        REQUIRE(gs.size() == 4);
        REQUIRE(gs[0] == Glyph{U'a', {.background = ColorIndex::Green}});
        REQUIRE(gs[1] == Glyph{U'b', {.background = ColorIndex::Green}});
        REQUIRE(gs[2] == Glyph{U'c', {.background = ColorIndex::Green}});
        REQUIRE(gs[3] == Glyph{U'd', {.background = ColorIndex::Red}});
    }

    SECTION("GS += Character")
    {
        SECTION("char")
        {
            auto gs = "abc" | bg(ColorIndex::Green);
            gs += 'd';

            REQUIRE(gs.size() == 4);
            REQUIRE(gs[0] == Glyph{U'a', {.background = ColorIndex::Green}});
            REQUIRE(gs[1] == Glyph{U'b', {.background = ColorIndex::Green}});
            REQUIRE(gs[2] == Glyph{U'c', {.background = ColorIndex::Green}});
            REQUIRE(gs[3] == Glyph{U'd'});
        }

        SECTION("char8_t")
        {
            auto gs = "abc" | bg(ColorIndex::Green);
            gs += u8'd';

            REQUIRE(gs.size() == 4);
            REQUIRE(gs[0] == Glyph{U'a', {.background = ColorIndex::Green}});
            REQUIRE(gs[1] == Glyph{U'b', {.background = ColorIndex::Green}});
            REQUIRE(gs[2] == Glyph{U'c', {.background = ColorIndex::Green}});
            REQUIRE(gs[3] == Glyph{U'd'});
        }

        SECTION("char16_t")
        {
            auto gs = "abc" | bg(ColorIndex::Green);
            gs += u'd';

            REQUIRE(gs.size() == 4);
            REQUIRE(gs[0] == Glyph{U'a', {.background = ColorIndex::Green}});
            REQUIRE(gs[1] == Glyph{U'b', {.background = ColorIndex::Green}});
            REQUIRE(gs[2] == Glyph{U'c', {.background = ColorIndex::Green}});
            REQUIRE(gs[3] == Glyph{U'd'});
        }

        SECTION("char32_t")
        {
            auto gs = "abc" | bg(ColorIndex::Green);
            gs += U'd';

            REQUIRE(gs.size() == 4);
            REQUIRE(gs[0] == Glyph{U'a', {.background = ColorIndex::Green}});
            REQUIRE(gs[1] == Glyph{U'b', {.background = ColorIndex::Green}});
            REQUIRE(gs[2] == Glyph{U'c', {.background = ColorIndex::Green}});
            REQUIRE(gs[3] == Glyph{U'd'});
        }

        SECTION("signed char")
        {
            auto gs = "abc" | bg(ColorIndex::Green);
            gs += static_cast<signed char>('d');

            REQUIRE(gs.size() == 4);
            REQUIRE(gs[0] == Glyph{U'a', {.background = ColorIndex::Green}});
            REQUIRE(gs[1] == Glyph{U'b', {.background = ColorIndex::Green}});
            REQUIRE(gs[2] == Glyph{U'c', {.background = ColorIndex::Green}});
            REQUIRE(gs[3] == Glyph{U'd'});
        }

        SECTION("unsigned char")
        {
            auto gs = "abc" | bg(ColorIndex::Green);
            gs += static_cast<unsigned char>('d');

            REQUIRE(gs.size() == 4);
            REQUIRE(gs[0] == Glyph{U'a', {.background = ColorIndex::Green}});
            REQUIRE(gs[1] == Glyph{U'b', {.background = ColorIndex::Green}});
            REQUIRE(gs[2] == Glyph{U'c', {.background = ColorIndex::Green}});
            REQUIRE(gs[3] == Glyph{U'd'});
        }
    }
}

TEST_CASE("operator+(..., ...)", "[Glyph]")
{
    SECTION("Glyph + Glyph")
    {
        auto const gs = Glyph{U'a'} + Glyph{U'b'};
        REQUIRE(gs.size() == 2);
        REQUIRE(gs[0] == Glyph{U'a'});
        REQUIRE(gs[1] == Glyph{U'b'});
    }

    SECTION("Glyph + Character", "[Glyph]")
    {
        SECTION("char")
        {
            auto const gs = Glyph{U'a'} + 'b';
            REQUIRE(gs.size() == 2);
            REQUIRE(gs[0] == Glyph{U'a'});
            REQUIRE(gs[1] == Glyph{U'b'});
        }

        SECTION("char8_t")
        {
            auto const gs = Glyph{U'a'} + u8'b';
            REQUIRE(gs.size() == 2);
            REQUIRE(gs[0] == Glyph{U'a'});
            REQUIRE(gs[1] == Glyph{U'b'});
        }

        SECTION("char16_t")
        {
            auto const gs = Glyph{U'a'} + u'b';
            REQUIRE(gs.size() == 2);
            REQUIRE(gs[0] == Glyph{U'a'});
            REQUIRE(gs[1] == Glyph{U'b'});
        }

        SECTION("char32_t")
        {
            auto const gs = Glyph{U'a'} + U'b';
            REQUIRE(gs.size() == 2);
            REQUIRE(gs[0] == Glyph{U'a'});
            REQUIRE(gs[1] == Glyph{U'b'});
        }

        SECTION("signed char")
        {
            auto const gs = Glyph{U'a'} + static_cast<signed char>('b');
            REQUIRE(gs.size() == 2);
            REQUIRE(gs[0] == Glyph{U'a'});
            REQUIRE(gs[1] == Glyph{U'b'});
        }

        SECTION("unsigned char")
        {
            auto const gs = Glyph{U'a'} + static_cast<unsigned char>('b');
            REQUIRE(gs.size() == 2);
            REQUIRE(gs[0] == Glyph{U'a'});
            REQUIRE(gs[1] == Glyph{U'b'});
        }
    }

    SECTION("Character + Glyph", "[Glyph]")
    {
        SECTION("char")
        {
            auto const gs = 'a' + Glyph{U'b'};
            REQUIRE(gs.size() == 2);
            REQUIRE(gs[0] == Glyph{U'a'});
            REQUIRE(gs[1] == Glyph{U'b'});
        }

        SECTION("char8_t")
        {
            auto const gs = u8'a' + Glyph{U'b'};
            REQUIRE(gs.size() == 2);
            REQUIRE(gs[0] == Glyph{U'a'});
            REQUIRE(gs[1] == Glyph{U'b'});
        }

        SECTION("char16_t")
        {
            auto const gs = u'a' + Glyph{U'b'};
            REQUIRE(gs.size() == 2);
            REQUIRE(gs[0] == Glyph{U'a'});
            REQUIRE(gs[1] == Glyph{U'b'});
        }

        SECTION("char32_t")
        {
            auto const gs = U'a' + Glyph{U'b'};
            REQUIRE(gs.size() == 2);
            REQUIRE(gs[0] == Glyph{U'a'});
            REQUIRE(gs[1] == Glyph{U'b'});
        }

        SECTION("signed char")
        {
            auto const gs = static_cast<signed char>('a') + Glyph{U'b'};
            REQUIRE(gs.size() == 2);
            REQUIRE(gs[0] == Glyph{U'a'});
            REQUIRE(gs[1] == Glyph{U'b'});
        }

        SECTION("unsigned char")
        {
            auto const gs = static_cast<unsigned char>('a') + Glyph{U'b'};
            REQUIRE(gs.size() == 2);
            REQUIRE(gs[0] == Glyph{U'a'});
            REQUIRE(gs[1] == Glyph{U'b'});
        }
    }

    SECTION("GlyphString + std::u32string_view")
    {
        auto const gs = ("abc" | bg(ColorIndex::Green)) + U"d😀f";
        REQUIRE(gs.size() == 6);
        REQUIRE(gs[0] == Glyph{U'a', {.background = ColorIndex::Green}});
        REQUIRE(gs[1] == Glyph{U'b', {.background = ColorIndex::Green}});
        REQUIRE(gs[2] == Glyph{U'c', {.background = ColorIndex::Green}});
        REQUIRE(gs[3] == Glyph{U'd'});
        REQUIRE(gs[4] == Glyph{U'😀'});
        REQUIRE(gs[5] == Glyph{U'f'});
    }

    SECTION("std::u32string_view + GlyphString")
    {
        auto const gs = U"a😀c" + ("def" | bg(ColorIndex::Green));
        REQUIRE(gs.size() == 6);
        REQUIRE(gs[0] == Glyph{U'a'});
        REQUIRE(gs[1] == Glyph{U'😀'});
        REQUIRE(gs[2] == Glyph{U'c'});
        REQUIRE(gs[3] == Glyph{U'd', {.background = ColorIndex::Green}});
        REQUIRE(gs[4] == Glyph{U'e', {.background = ColorIndex::Green}});
        REQUIRE(gs[5] == Glyph{U'f', {.background = ColorIndex::Green}});
    }

    SECTION("GlyphString + std::string_view")
    {
        auto const gs = ("abc" | bg(ColorIndex::Green)) + "d😀f";
        REQUIRE(gs.size() == 6);
        REQUIRE(gs[0] == Glyph{U'a', {.background = ColorIndex::Green}});
        REQUIRE(gs[1] == Glyph{U'b', {.background = ColorIndex::Green}});
        REQUIRE(gs[2] == Glyph{U'c', {.background = ColorIndex::Green}});
        REQUIRE(gs[3] == Glyph{U'd'});
        REQUIRE(gs[4] == Glyph{U'😀'});
        REQUIRE(gs[5] == Glyph{U'f'});
    }

    SECTION("std::string_view + GlyphString")
    {
        auto const gs = "a😀c" + ("def" | bg(ColorIndex::Green));
        REQUIRE(gs.size() == 6);
        REQUIRE(gs[0] == Glyph{U'a'});
        REQUIRE(gs[1] == Glyph{U'😀'});
        REQUIRE(gs[2] == Glyph{U'c'});
        REQUIRE(gs[3] == Glyph{U'd', {.background = ColorIndex::Green}});
        REQUIRE(gs[4] == Glyph{U'e', {.background = ColorIndex::Green}});
        REQUIRE(gs[5] == Glyph{U'f', {.background = ColorIndex::Green}});
    }

    SECTION("GlyphString + GlyphString")
    {
        auto const gs =
            ("abc" | bg(ColorIndex::Green)) + ("d😀f" | bg(ColorIndex::Red));
        REQUIRE(gs.size() == 6);
        REQUIRE(gs[0] == Glyph{U'a', {.background = ColorIndex::Green}});
        REQUIRE(gs[1] == Glyph{U'b', {.background = ColorIndex::Green}});
        REQUIRE(gs[2] == Glyph{U'c', {.background = ColorIndex::Green}});
        REQUIRE(gs[3] == Glyph{U'd', {.background = ColorIndex::Red}});
        REQUIRE(gs[4] == Glyph{U'😀', {.background = ColorIndex::Red}});
        REQUIRE(gs[5] == Glyph{U'f', {.background = ColorIndex::Red}});
    }

    SECTION("GlyphString + Glyph")
    {
        auto const gs = ("abc" | bg(ColorIndex::Green)) + Glyph{U'd'};
        REQUIRE(gs.size() == 4);
        REQUIRE(gs[0] == Glyph{U'a', {.background = ColorIndex::Green}});
        REQUIRE(gs[1] == Glyph{U'b', {.background = ColorIndex::Green}});
        REQUIRE(gs[2] == Glyph{U'c', {.background = ColorIndex::Green}});
        REQUIRE(gs[3] == Glyph{U'd'});
    }

    SECTION("Glyph + GlyphString")
    {
        auto const gs = Glyph{U'a'} + ("def" | bg(ColorIndex::Green));
        REQUIRE(gs.size() == 4);
        REQUIRE(gs[0] == Glyph{U'a'});
        REQUIRE(gs[1] == Glyph{U'd', {.background = ColorIndex::Green}});
        REQUIRE(gs[2] == Glyph{U'e', {.background = ColorIndex::Green}});
        REQUIRE(gs[3] == Glyph{U'f', {.background = ColorIndex::Green}});
    }

    SECTION("Glyph + std::string_view")
    {
        auto const gs = Glyph{U'a'} + "b😀d";
        REQUIRE(gs.size() == 4);
        REQUIRE(gs[0] == Glyph{U'a'});
        REQUIRE(gs[1] == Glyph{U'b'});
        REQUIRE(gs[2] == Glyph{U'😀'});
        REQUIRE(gs[3] == Glyph{U'd'});
    }

    SECTION("Glyph + std::u32string_view")
    {
        auto const gs = Glyph{U'a'} + U"b😀d";
        REQUIRE(gs.size() == 4);
        REQUIRE(gs[0] == Glyph{U'a'});
        REQUIRE(gs[1] == Glyph{U'b'});
        REQUIRE(gs[2] == Glyph{U'😀'});
        REQUIRE(gs[3] == Glyph{U'd'});
    }

    SECTION("std::string_view + Glyph")
    {
        auto const gs = "a😀b" + Glyph{U'c'};
        REQUIRE(gs.size() == 4);
        REQUIRE(gs[0] == Glyph{U'a'});
        REQUIRE(gs[1] == Glyph{U'😀'});
        REQUIRE(gs[2] == Glyph{U'b'});
        REQUIRE(gs[3] == Glyph{U'c'});
    }

    SECTION("std::u32string_view + Glyph")
    {
        auto const gs = U"a😀b" + Glyph{U'c'};
        REQUIRE(gs.size() == 4);
        REQUIRE(gs[0] == Glyph{U'a'});
        REQUIRE(gs[1] == Glyph{U'😀'});
        REQUIRE(gs[2] == Glyph{U'b'});
        REQUIRE(gs[3] == Glyph{U'c'});
    }
}