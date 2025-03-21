#include <deque>
#include <list>
#include <string>
#include <vector>

#define TEST_MAIN
#include <zzz/test.hpp>

#include <esc/glyph.hpp>

using namespace esc;

TEST(glyph_constructor)
{
    {  // Default Constructor
        auto const glyph = Glyph{};
        ASSERT(glyph.symbol == U' ');
        ASSERT(glyph.brush == Brush{});
    }

    {  // Symbol Constructor
        auto const glyph = Glyph{U'x'};
        ASSERT(glyph.symbol == U'x');
        ASSERT(glyph.brush == Brush{});
    }

    {  // Symbol, Brush Constructor
        auto const glyph = Glyph{U'x', {.background = XColor::Red}};
        ASSERT(glyph.symbol == U'x');
        ASSERT(glyph.brush == Brush{.background = XColor::Red});
    }
}

TEST(pipe_operators)
{
    {  // char
        ASSERT((('A' | bg(XColor::Red)) == Glyph{U'A', {.background = XColor::Red}}));
        ASSERT((('A' | fg(XColor::Red)) == Glyph{U'A', {.foreground = XColor::Red}}));
        ASSERT((('A' | Trait::Bold) == Glyph{U'A', {.traits = Trait::Bold}}));
    }

    {  // signed char
        auto const ch = static_cast<signed char>('B');
        ASSERT(((ch | bg(XColor::Red)) == Glyph{U'B', {.background = XColor::Red}}));
        ASSERT(((ch | fg(XColor::Red)) == Glyph{U'B', {.foreground = XColor::Red}}));
        ASSERT(((ch | Trait::Bold) == Glyph{U'B', {.traits = Trait::Bold}}));
    }

    {  // unsigned char
        auto const ch = static_cast<unsigned char>('C');
        ASSERT(((ch | bg(XColor::Red)) == Glyph{U'C', {.background = XColor::Red}}));
        ASSERT(((ch | fg(XColor::Red)) == Glyph{U'C', {.foreground = XColor::Red}}));
        ASSERT(((ch | Trait::Bold) == Glyph{U'C', {.traits = Trait::Bold}}));
    }

    {  // char8_t
        auto const ch = u8'D';
        ASSERT(((ch | bg(XColor::Red)) == Glyph{U'D', {.background = XColor::Red}}));
        ASSERT(((ch | fg(XColor::Red)) == Glyph{U'D', {.foreground = XColor::Red}}));
        ASSERT(((ch | Trait::Bold) == Glyph{U'D', {.traits = Trait::Bold}}));
    }

    {  // char16_t
        auto const ch = u'E';
        ASSERT(((ch | bg(XColor::Red)) == Glyph{U'E', {.background = XColor::Red}}));
        ASSERT(((ch | fg(XColor::Red)) == Glyph{U'E', {.foreground = XColor::Red}}));
        ASSERT(((ch | Trait::Bold) == Glyph{U'E', {.traits = Trait::Bold}}));
    }

    {  // char32_t
        auto const ch = U'F';
        ASSERT(((ch | bg(XColor::Red)) == Glyph{U'F', {.background = XColor::Red}}));
        ASSERT(((ch | fg(XColor::Red)) == Glyph{U'F', {.foreground = XColor::Red}}));
        ASSERT(((ch | Trait::Bold) == Glyph{U'F', {.traits = Trait::Bold}}));
    }
}

template <typename Container>
void test_glyphstring_pipe_ops_lval()
{
    {  // ColorBG
        auto glyphs = Container(5, {U'a', {.traits = Trait::Bold}});
        glyphs | bg(XColor::Green);

        for (auto const& glyph : glyphs) {
            ASSERT((glyph ==
                    Glyph{U'a', {.background = XColor::Green, .traits = Trait::Bold}}));
        }
    }

    {  // ColorFG
        auto glyphs = Container(5, {U'a', {.traits = Trait::Bold}});
        glyphs | fg(XColor::Green);

        for (auto const& glyph : glyphs) {
            ASSERT((glyph ==
                    Glyph{U'a', {.foreground = XColor::Green, .traits = Trait::Bold}}));
        }
    }

    {  // Trait
        auto glyphs = Container(5, {U'a', {.traits = Trait::Bold}});
        glyphs | Trait::Italic;

        for (auto const& glyph : glyphs) {
            ASSERT((glyph == Glyph{U'a', {.traits = Trait::Bold | Trait::Italic}}));
        }
    }

    {  // RemoveTrait
        auto glyphs = Container(5, {U'a', {.traits = Trait::Bold}});
        glyphs | remove_trait(Trait::Bold);

        for (auto const& glyph : glyphs) {
            ASSERT(glyph == Glyph{U'a'});
        }
    }

    {  // ColorBG, ColorFG, Trait
        auto glyphs = Container(5, {U'a', {.traits = Trait::Bold}});
        glyphs | bg(XColor::Green) | fg(XColor::Red) | Trait::Dim;

        for (auto const& glyph : glyphs) {
            ASSERT((glyph == Glyph{U'a',
                                   {.background = XColor::Green,
                                    .foreground = XColor::Red,
                                    .traits = Trait::Bold | Trait::Dim}}));
        }
    }
}

template <typename Container>
void test_glyphstring_pipe_ops_rval()
{
    {  // ColorBG
        auto const glyphs =
            Container(5, {U'a', {.traits = Trait::Bold}}) | bg(XColor::Green);

        for (auto const& glyph : glyphs) {
            ASSERT((glyph ==
                    Glyph{U'a', {.background = XColor::Green, .traits = Trait::Bold}}));
        }
    }

    {  // ColorFG
        auto const glyphs =
            Container(5, {U'a', {.traits = Trait::Bold}}) | fg(XColor::Green);

        for (auto const& glyph : glyphs) {
            ASSERT((glyph ==
                    Glyph{U'a', {.foreground = XColor::Green, .traits = Trait::Bold}}));
        }
    }

    {  // Trait
        auto const glyphs =
            Container(5, {U'a', {.traits = Trait::Bold}}) | Trait::Italic;

        for (auto const& glyph : glyphs) {
            ASSERT((glyph == Glyph{U'a', {.traits = Trait::Bold | Trait::Italic}}));
        }
    }

    {  // RemoveTrait
        auto const glyphs =
            Container(5, {U'a', {.traits = Trait::Bold}}) | remove_trait(Trait::Bold);

        for (auto const& glyph : glyphs) {
            ASSERT(glyph == Glyph{U'a'});
        }
    }

    {  // ColorBG, ColorFG, Trait
        auto const glyphs = Container(5, {U'a', {.traits = Trait::Bold}}) |
                            bg(XColor::Green) | fg(XColor::Red) | Trait::Dim;

        for (auto const& glyph : glyphs) {
            ASSERT((glyph == Glyph{U'a',
                                   {.background = XColor::Green,
                                    .foreground = XColor::Red,
                                    .traits = Trait::Bold | Trait::Dim}}));
        }
    }
}

TEST(vector_glyphstring_pipe_ops)
{
    test_glyphstring_pipe_ops_lval<std::vector<Glyph>>();
    test_glyphstring_pipe_ops_rval<std::vector<Glyph>>();
}

TEST(list_glyphstring_pipe_ops)
{
    test_glyphstring_pipe_ops_lval<std::list<Glyph>>();
    test_glyphstring_pipe_ops_rval<std::list<Glyph>>();
}

TEST(deque_glyphstring_pipe_ops)
{
    test_glyphstring_pipe_ops_lval<std::deque<Glyph>>();
    test_glyphstring_pipe_ops_rval<std::deque<Glyph>>();
}

template <typename StringType>
void test_string_pipe_ops(StringType abc)
{
    {  // ColorBG
        auto const gs = abc | bg(XColor::Green);
        ASSERT(gs.size() == 3);
        ASSERT((gs[0] == Glyph{U'a', {.background = XColor::Green}}));
        ASSERT((gs[1] == Glyph{U'b', {.background = XColor::Green}}));
        ASSERT((gs[2] == Glyph{U'c', {.background = XColor::Green}}));
    }

    {  // ColorFG
        auto const gs = abc | fg(XColor::Green);
        ASSERT(gs.size() == 3);
        ASSERT((gs[0] == Glyph{U'a', {.foreground = XColor::Green}}));
        ASSERT((gs[1] == Glyph{U'b', {.foreground = XColor::Green}}));
        ASSERT((gs[2] == Glyph{U'c', {.foreground = XColor::Green}}));
    }

    {  // Trait
        auto const gs = abc | Trait::Bold;
        ASSERT(gs.size() == 3);
        ASSERT((gs[0] == Glyph{U'a', {.traits = Trait::Bold}}));
        ASSERT((gs[1] == Glyph{U'b', {.traits = Trait::Bold}}));
        ASSERT((gs[2] == Glyph{U'c', {.traits = Trait::Bold}}));
    }

    {  // RemoveTrait
        auto const gs = abc | Trait::Bold | remove_trait(Trait::Bold);
        ASSERT(gs.size() == 3);
        ASSERT(gs[0] == Glyph{U'a'});
        ASSERT(gs[1] == Glyph{U'b'});
        ASSERT(gs[2] == Glyph{U'c'});
    }

    {  // ColorBG, ColorFG, Trait
        auto const gs = abc | bg(XColor::Green) | fg(XColor::Red) | Trait::Dim;
        ASSERT(gs.size() == 3);
        auto const brush = Brush{.background = XColor::Green,
                                 .foreground = XColor::Red,
                                 .traits = Trait::Dim};
        ASSERT((gs[0] == Glyph{U'a', brush}));
        ASSERT((gs[1] == Glyph{U'b', brush}));
        ASSERT((gs[2] == Glyph{U'c', brush}));
    }
}

TEST(string_pipe_operators)
{
    test_string_pipe_ops(U"abc");
    test_string_pipe_ops(std::u32string{U"abc"});
    test_string_pipe_ops("abc");
    test_string_pipe_ops(std::string{"abc"});
}

TEST(pipe_ops_multibyte_symbols)
{
    {  // char32_t const*
        auto const gs = U"🚦" | bg(XColor::Green);
        ASSERT(gs.size() == 1);
        ASSERT((gs[0] == Glyph{U'🚦', {.background = XColor::Green}}));
    }

    {  // Empty char32_t const*
        auto const gs = U"" | bg(XColor::Green);
        ASSERT(gs.empty());
    }

    {  // std::u32string_view
        auto const gs = std::u32string_view{U"A¢€😀 世界𐍈"} | bg(XColor::Green);

        ASSERT(gs.size() == 8);

        ASSERT((gs[1] == Glyph{U'¢', {.background = XColor::Green}}));
        ASSERT((gs[2] == Glyph{U'€', {.background = XColor::Green}}));
        ASSERT((gs[3] == Glyph{U'😀', {.background = XColor::Green}}));
        ASSERT((gs[4] == Glyph{U' ', {.background = XColor::Green}}));
        ASSERT((gs[5] == Glyph{U'世', {.background = XColor::Green}}));
        ASSERT((gs[6] == Glyph{U'界', {.background = XColor::Green}}));
        ASSERT((gs[7] == Glyph{U'𐍈', {.background = XColor::Green}}));
    }

    {  // char const*
        auto const gs = "🚦" | bg(XColor::Green);
        ASSERT(gs.size() == 1);
        ASSERT((gs[0] == Glyph{U'🚦', {.background = XColor::Green}}));
    }

    {  // Empty char const*
        auto const gs = "" | bg(XColor::Green);
        ASSERT(gs.empty());
    }

    {  // std::string_view
        auto const gs = std::string_view{"A¢€😀 世界𐍈"} | bg(XColor::Green);

        ASSERT(gs.size() == 8);

        ASSERT((gs[1] == Glyph{U'¢', {.background = XColor::Green}}));
        ASSERT((gs[2] == Glyph{U'€', {.background = XColor::Green}}));
        ASSERT((gs[3] == Glyph{U'😀', {.background = XColor::Green}}));
        ASSERT((gs[4] == Glyph{U' ', {.background = XColor::Green}}));
        ASSERT((gs[5] == Glyph{U'世', {.background = XColor::Green}}));
        ASSERT((gs[6] == Glyph{U'界', {.background = XColor::Green}}));
        ASSERT((gs[7] == Glyph{U'𐍈', {.background = XColor::Green}}));
    }
}

TEST(chained_pipe_ops)
{
    {  // Glyph
        auto const g = Glyph{U'a'} | bg(XColor::Green) | fg(XColor::Red) | Trait::Bold |
                       Trait::Italic | remove_trait(Trait::Bold);
        ASSERT((g == Glyph{.symbol = U'a',
                           .brush = {.background = XColor::Green,
                                     .foreground = XColor::Red,
                                     .traits = Trait::Italic}}));
    }

    {  // GlyphString
        auto const gs = "abc" | bg(XColor::Green) | fg(XColor::Red) | Trait::Bold |
                        Trait::Italic | remove_trait(Trait::Bold);

        ASSERT(gs.size() == 3);
        ASSERT((gs[0] == Glyph{.symbol = U'a',
                               .brush = {.background = XColor::Green,
                                         .foreground = XColor::Red,
                                         .traits = Trait::Italic}}));
        ASSERT((gs[1] == Glyph{.symbol = U'b',
                               .brush = {.background = XColor::Green,
                                         .foreground = XColor::Red,
                                         .traits = Trait::Italic}}));
        ASSERT((gs[2] == Glyph{.symbol = U'c',
                               .brush = {.background = XColor::Green,
                                         .foreground = XColor::Red,
                                         .traits = Trait::Italic}}));
    }
}

TEST(op_compound_plus_assignment_glyphstring)
{
    {  // GS += GS
        auto gs = "abc" | bg(XColor::Green);
        gs += "def" | bg(XColor::Red);

        ASSERT(gs.size() == 6);
        ASSERT((gs[0] == Glyph{U'a', {.background = XColor::Green}}));
        ASSERT((gs[1] == Glyph{U'b', {.background = XColor::Green}}));
        ASSERT((gs[2] == Glyph{U'c', {.background = XColor::Green}}));
        ASSERT((gs[3] == Glyph{U'd', {.background = XColor::Red}}));
        ASSERT((gs[4] == Glyph{U'e', {.background = XColor::Red}}));
        ASSERT((gs[5] == Glyph{U'f', {.background = XColor::Red}}));
    }

    {  // GS += std::u32string_view
        auto gs = "abc" | bg(XColor::Green);
        gs += U"d😀f";

        ASSERT(gs.size() == 6);
        ASSERT((gs[0] == Glyph{U'a', {.background = XColor::Green}}));
        ASSERT((gs[1] == Glyph{U'b', {.background = XColor::Green}}));
        ASSERT((gs[2] == Glyph{U'c', {.background = XColor::Green}}));
        ASSERT(gs[3] == Glyph{U'd'});
        ASSERT(gs[4] == Glyph{U'😀'});
        ASSERT(gs[5] == Glyph{U'f'});
    }

    {  // GS += std::string_view
        auto gs = "abc" | bg(XColor::Green);
        gs += "d😀f";

        ASSERT(gs.size() == 6);
        ASSERT((gs[0] == Glyph{U'a', {.background = XColor::Green}}));
        ASSERT((gs[1] == Glyph{U'b', {.background = XColor::Green}}));
        ASSERT((gs[2] == Glyph{U'c', {.background = XColor::Green}}));
        ASSERT(gs[3] == Glyph{U'd'});
        ASSERT(gs[4] == Glyph{U'😀'});
        ASSERT(gs[5] == Glyph{U'f'});
    }

    {  // GS += Glyph
        auto gs = "abc" | bg(XColor::Green);
        gs += Glyph{U'd', {.background = XColor::Red}};

        ASSERT(gs.size() == 4);
        ASSERT((gs[0] == Glyph{U'a', {.background = XColor::Green}}));
        ASSERT((gs[1] == Glyph{U'b', {.background = XColor::Green}}));
        ASSERT((gs[2] == Glyph{U'c', {.background = XColor::Green}}));
        ASSERT((gs[3] == Glyph{U'd', {.background = XColor::Red}}));
    }

    {  // GS += char
        auto gs = "abc" | bg(XColor::Green);
        gs += 'd';

        ASSERT(gs.size() == 4);
        ASSERT((gs[0] == Glyph{U'a', {.background = XColor::Green}}));
        ASSERT((gs[1] == Glyph{U'b', {.background = XColor::Green}}));
        ASSERT((gs[2] == Glyph{U'c', {.background = XColor::Green}}));
        ASSERT(gs[3] == Glyph{U'd'});
    }

    {  // GS += char8_t
        auto gs = "abc" | bg(XColor::Green);
        gs += u8'd';

        ASSERT(gs.size() == 4);
        ASSERT((gs[0] == Glyph{U'a', {.background = XColor::Green}}));
        ASSERT((gs[1] == Glyph{U'b', {.background = XColor::Green}}));
        ASSERT((gs[2] == Glyph{U'c', {.background = XColor::Green}}));
        ASSERT(gs[3] == Glyph{U'd'});
    }

    {  // GS += char16_t
        auto gs = "abc" | bg(XColor::Green);
        gs += u'd';

        ASSERT(gs.size() == 4);
        ASSERT((gs[0] == Glyph{U'a', {.background = XColor::Green}}));
        ASSERT((gs[1] == Glyph{U'b', {.background = XColor::Green}}));
        ASSERT((gs[2] == Glyph{U'c', {.background = XColor::Green}}));
        ASSERT(gs[3] == Glyph{U'd'});
    }

    {  // GS += char32_t
        auto gs = "abc" | bg(XColor::Green);
        gs += U'd';

        ASSERT(gs.size() == 4);
        ASSERT((gs[0] == Glyph{U'a', {.background = XColor::Green}}));
        ASSERT((gs[1] == Glyph{U'b', {.background = XColor::Green}}));
        ASSERT((gs[2] == Glyph{U'c', {.background = XColor::Green}}));
        ASSERT(gs[3] == Glyph{U'd'});
    }

    {  // GS += signed char
        auto gs = "abc" | bg(XColor::Green);
        gs += static_cast<signed char>('d');

        ASSERT(gs.size() == 4);
        ASSERT((gs[0] == Glyph{U'a', {.background = XColor::Green}}));
        ASSERT((gs[1] == Glyph{U'b', {.background = XColor::Green}}));
        ASSERT((gs[2] == Glyph{U'c', {.background = XColor::Green}}));
        ASSERT(gs[3] == Glyph{U'd'});
    }

    {  // GS += unsigned char
        auto gs = "abc" | bg(XColor::Green);
        gs += static_cast<unsigned char>('d');

        ASSERT(gs.size() == 4);
        ASSERT((gs[0] == Glyph{U'a', {.background = XColor::Green}}));
        ASSERT((gs[1] == Glyph{U'b', {.background = XColor::Green}}));
        ASSERT((gs[2] == Glyph{U'c', {.background = XColor::Green}}));
        ASSERT(gs[3] == Glyph{U'd'});
    }
}

TEST(op_plus_any)
{
    {  // Glyph + Glyph
        auto const gs = Glyph{U'a'} + Glyph{U'b'};
        ASSERT(gs.size() == 2);
        ASSERT(gs[0] == Glyph{U'a'});
        ASSERT(gs[1] == Glyph{U'b'});
    }

    {      // Glyph + Character
        {  // char
            auto const gs = Glyph{U'a'} + 'b';
            ASSERT(gs.size() == 2);
            ASSERT(gs[0] == Glyph{U'a'});
            ASSERT(gs[1] == Glyph{U'b'});
        }

        {  // char8_t
            auto const gs = Glyph{U'a'} + u8'b';
            ASSERT(gs.size() == 2);
            ASSERT(gs[0] == Glyph{U'a'});
            ASSERT(gs[1] == Glyph{U'b'});
        }

        {  // char16_t
            auto const gs = Glyph{U'a'} + u'b';
            ASSERT(gs.size() == 2);
            ASSERT(gs[0] == Glyph{U'a'});
            ASSERT(gs[1] == Glyph{U'b'});
        }

        {  // char32_t
            auto const gs = Glyph{U'a'} + U'b';
            ASSERT(gs.size() == 2);
            ASSERT(gs[0] == Glyph{U'a'});
            ASSERT(gs[1] == Glyph{U'b'});
        }

        {  // signed char
            auto const gs = Glyph{U'a'} + static_cast<signed char>('b');
            ASSERT(gs.size() == 2);
            ASSERT(gs[0] == Glyph{U'a'});
            ASSERT(gs[1] == Glyph{U'b'});
        }

        {  // unsigned char
            auto const gs = Glyph{U'a'} + static_cast<unsigned char>('b');
            ASSERT(gs.size() == 2);
            ASSERT(gs[0] == Glyph{U'a'});
            ASSERT(gs[1] == Glyph{U'b'});
        }
    }

    {      // Character + Glyph
        {  // char
            auto const gs = 'a' + Glyph{U'b'};
            ASSERT(gs.size() == 2);
            ASSERT(gs[0] == Glyph{U'a'});
            ASSERT(gs[1] == Glyph{U'b'});
        }

        {  // char8_t
            auto const gs = u8'a' + Glyph{U'b'};
            ASSERT(gs.size() == 2);
            ASSERT(gs[0] == Glyph{U'a'});
            ASSERT(gs[1] == Glyph{U'b'});
        }

        {  // char16_t
            auto const gs = u'a' + Glyph{U'b'};
            ASSERT(gs.size() == 2);
            ASSERT(gs[0] == Glyph{U'a'});
            ASSERT(gs[1] == Glyph{U'b'});
        }

        {  // char32_t
            auto const gs = U'a' + Glyph{U'b'};
            ASSERT(gs.size() == 2);
            ASSERT(gs[0] == Glyph{U'a'});
            ASSERT(gs[1] == Glyph{U'b'});
        }

        {  // signed char
            auto const gs = static_cast<signed char>('a') + Glyph{U'b'};
            ASSERT(gs.size() == 2);
            ASSERT(gs[0] == Glyph{U'a'});
            ASSERT(gs[1] == Glyph{U'b'});
        }

        {  // unsigned char
            auto const gs = static_cast<unsigned char>('a') + Glyph{U'b'};
            ASSERT(gs.size() == 2);
            ASSERT(gs[0] == Glyph{U'a'});
            ASSERT(gs[1] == Glyph{U'b'});
        }
    }

    {  // GlyphString + std::u32string_view
        auto const gs = ("abc" | bg(XColor::Green)) + U"d😀f";
        ASSERT(gs.size() == 6);
        ASSERT((gs[0] == Glyph{U'a', {.background = XColor::Green}}));
        ASSERT((gs[1] == Glyph{U'b', {.background = XColor::Green}}));
        ASSERT((gs[2] == Glyph{U'c', {.background = XColor::Green}}));
        ASSERT(gs[3] == Glyph{U'd'});
        ASSERT(gs[4] == Glyph{U'😀'});
        ASSERT(gs[5] == Glyph{U'f'});
    }

    {  // std::u32string_view + GlyphString
        auto const gs = U"a😀c" + ("def" | bg(XColor::Green));
        ASSERT(gs.size() == 6);
        ASSERT(gs[0] == Glyph{U'a'});
        ASSERT(gs[1] == Glyph{U'😀'});
        ASSERT(gs[2] == Glyph{U'c'});
        ASSERT((gs[3] == Glyph{U'd', {.background = XColor::Green}}));
        ASSERT((gs[4] == Glyph{U'e', {.background = XColor::Green}}));
        ASSERT((gs[5] == Glyph{U'f', {.background = XColor::Green}}));
    }

    {  // GlyphString + std::string_view
        auto const gs = ("abc" | bg(XColor::Green)) + "d😀f";
        ASSERT(gs.size() == 6);
        ASSERT((gs[0] == Glyph{U'a', {.background = XColor::Green}}));
        ASSERT((gs[1] == Glyph{U'b', {.background = XColor::Green}}));
        ASSERT((gs[2] == Glyph{U'c', {.background = XColor::Green}}));
        ASSERT(gs[3] == Glyph{U'd'});
        ASSERT(gs[4] == Glyph{U'😀'});
        ASSERT(gs[5] == Glyph{U'f'});
    }

    {  // std::string_view + GlyphString
        auto const gs = "a😀c" + ("def" | bg(XColor::Green));
        ASSERT(gs.size() == 6);
        ASSERT(gs[0] == Glyph{U'a'});
        ASSERT(gs[1] == Glyph{U'😀'});
        ASSERT(gs[2] == Glyph{U'c'});
        ASSERT((gs[3] == Glyph{U'd', {.background = XColor::Green}}));
        ASSERT((gs[4] == Glyph{U'e', {.background = XColor::Green}}));
        ASSERT((gs[5] == Glyph{U'f', {.background = XColor::Green}}));
    }

    {  // GlyphString + GlyphString
        auto const gs = ("abc" | bg(XColor::Green)) + ("d😀f" | bg(XColor::Red));
        ASSERT(gs.size() == 6);
        ASSERT((gs[0] == Glyph{U'a', {.background = XColor::Green}}));
        ASSERT((gs[1] == Glyph{U'b', {.background = XColor::Green}}));
        ASSERT((gs[2] == Glyph{U'c', {.background = XColor::Green}}));
        ASSERT((gs[3] == Glyph{U'd', {.background = XColor::Red}}));
        ASSERT((gs[4] == Glyph{U'😀', {.background = XColor::Red}}));
        ASSERT((gs[5] == Glyph{U'f', {.background = XColor::Red}}));
    }

    {  // GlyphString + Glyph
        auto const gs = ("abc" | bg(XColor::Green)) + Glyph{U'd'};
        ASSERT(gs.size() == 4);
        ASSERT((gs[0] == Glyph{U'a', {.background = XColor::Green}}));
        ASSERT((gs[1] == Glyph{U'b', {.background = XColor::Green}}));
        ASSERT((gs[2] == Glyph{U'c', {.background = XColor::Green}}));
        ASSERT(gs[3] == Glyph{U'd'});
    }

    {  // GlyphString + Character
        auto const gs = ("abc" | bg(XColor::Green)) + 'd';
        ASSERT(gs.size() == 4);
        ASSERT((gs[0] == Glyph{U'a', {.background = XColor::Green}}));
        ASSERT((gs[1] == Glyph{U'b', {.background = XColor::Green}}));
        ASSERT((gs[2] == Glyph{U'c', {.background = XColor::Green}}));
        ASSERT(gs[3] == Glyph{U'd'});
    }

    {  // Glyph + GlyphString
        auto const gs = Glyph{U'a'} + ("def" | bg(XColor::Green));
        ASSERT(gs.size() == 4);
        ASSERT(gs[0] == Glyph{U'a'});
        ASSERT((gs[1] == Glyph{U'd', {.background = XColor::Green}}));
        ASSERT((gs[2] == Glyph{U'e', {.background = XColor::Green}}));
        ASSERT((gs[3] == Glyph{U'f', {.background = XColor::Green}}));
    }

    {  // Character + GlyphString
        auto const gs = U'a' + ("def" | bg(XColor::Green));
        ASSERT(gs.size() == 4);
        ASSERT(gs[0] == Glyph{U'a'});
        ASSERT((gs[1] == Glyph{U'd', {.background = XColor::Green}}));
        ASSERT((gs[2] == Glyph{U'e', {.background = XColor::Green}}));
        ASSERT((gs[3] == Glyph{U'f', {.background = XColor::Green}}));
    }

    {  // Glyph + std::string_view
        auto const gs = Glyph{U'a'} + "def";
        ASSERT(gs.size() == 4);
        ASSERT(gs[0] == Glyph{U'a'});
        ASSERT(gs[1] == Glyph{U'd'});
        ASSERT(gs[2] == Glyph{U'e'});
        ASSERT(gs[3] == Glyph{U'f'});
    }

    {  // Glyph + std::u32string_view
        auto const gs = Glyph{U'a'} + U"def";
        ASSERT(gs.size() == 4);
        ASSERT(gs[0] == Glyph{U'a'});
        ASSERT(gs[1] == Glyph{U'd'});
        ASSERT(gs[2] == Glyph{U'e'});
        ASSERT(gs[3] == Glyph{U'f'});
    }

    {  // std::string_view + Glyph
        auto const gs = "abc" + Glyph{U'd'};
        ASSERT(gs.size() == 4);
        ASSERT(gs[0] == Glyph{U'a'});
        ASSERT(gs[1] == Glyph{U'b'});
        ASSERT(gs[2] == Glyph{U'c'});
        ASSERT(gs[3] == Glyph{U'd'});
    }

    {  // std::u32string_view + Glyph
        auto const gs = U"abc" + Glyph{U'd'};
        ASSERT(gs.size() == 4);
        ASSERT(gs[0] == Glyph{U'a'});
        ASSERT(gs[1] == Glyph{U'b'});
        ASSERT(gs[2] == Glyph{U'c'});
        ASSERT(gs[3] == Glyph{U'd'});
    }
}