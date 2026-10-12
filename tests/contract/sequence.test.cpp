#include "support/encoding.hpp"

#include <climits>
#include <stdexcept>
#include <type_traits>

using namespace esc;
using contract::encodes;

static_assert(!std::same_as<Cursor, Point>);
static_assert(DrawingCommand<Cursor> && DrawingCommand<Cursor const&>);
static_assert(!DrawingCommand<Point> && !DrawingCommand<int>);
static_assert(!DrawingCommand<Color> && !DrawingCommand<std::string>);
static_assert(Escapable<ScreenBuffer> && !DrawingCommand<ScreenBuffer>);
static_assert(!Escapable<Echo> && !Escapable<MouseMode>);
static_assert(std::same_as<decltype(encode(BlankRow{})), std::string>);

TEST(ENC_01_absolute_cursor_uses_row_then_column)
{
    encodes("\x1b[1;1H", Cursor{0,0});
    encodes("\x1b[3;5H", Cursor{4,2});
    encodes("\x1b[8;12H", cursor(Point{11,7}));
}
TEST(ENC_04_cursor_extremes_do_not_overflow)
{
    static_assert(INT_MAX == 2147483647, "These fixtures target the supported 32-bit-int backends.");
    encodes("\x1b[2147483648;2147483648H", Cursor{INT_MAX,INT_MAX});
    encodes("\x1b[2147483648A\x1b[2147483648D", MoveCursor{INT_MIN,INT_MIN});
    CHECK_THROWS(std::invalid_argument, encode(Cursor{-1,0}));
    CHECK_THROWS(std::invalid_argument, encode(Cursor{0,-1}));
}
TEST(ENC_01_relative_cursor_is_vertical_then_horizontal)
{
    encodes("", MoveCursor{});
    encodes("\x1b[2B\x1b[3C", MoveCursor{3,2});
    encodes("\x1b[2A\x1b[3D", MoveCursor{-3,-2});
    encodes("\x1b[7C", MoveCursor{7,0});
    encodes("\x1b[7A", MoveCursor{0,-7});
}
TEST(ENC_01_erase_scope_does_not_home_or_clear_history)
{
    encodes("\x1b[2K", BlankRow{});
    encodes("\x1b[0K", BlankRow{Erase::ToEnd});
    encodes("\x1b[1K", BlankRow{Erase::ToBeginning});
    encodes("\x1b[2J", BlankScreen{});
    encodes("\x1b[0J", BlankScreen{Erase::ToEnd});
    encodes("\x1b[1J", BlankScreen{Erase::ToBeginning});
    CHECK_THROWS(std::invalid_argument, encode(BlankScreen{static_cast<Erase>(99)}));
}
TEST(ENC_01_default_and_named_colors)
{
    encodes("\x1b[39m", fg(default_color));
    encodes("\x1b[49m", bg(default_color));
    encodes("\x1b[30m", fg(colors::black));
    encodes("\x1b[31m", fg(colors::red));
    encodes("\x1b[46m", bg(colors::cyan));
    encodes("\x1b[97m", fg(colors::bright_white));
    encodes("\x1b[100m", bg(colors::bright_black));
}
TEST(ENC_01_every_index_has_its_canonical_encoding)
{
    // Independent wire formula specified by the document, not calls into Escape.
    for (unsigned index=0; index<256; ++index) {
        auto fg_bytes = index<8 ? "\x1b["+std::to_string(30+index)+"m"
                      : index<16 ? "\x1b["+std::to_string(90+index-8)+"m"
                      : "\x1b[38;5;"+std::to_string(index)+"m";
        auto bg_bytes = index<8 ? "\x1b["+std::to_string(40+index)+"m"
                      : index<16 ? "\x1b["+std::to_string(100+index-8)+"m"
                      : "\x1b[48;5;"+std::to_string(index)+"m";
        encodes(fg_bytes, fg(IndexedColor{static_cast<std::uint8_t>(index)}));
        encodes(bg_bytes, bg(IndexedColor{static_cast<std::uint8_t>(index)}));
    }
}
TEST(ENC_01_true_color_preserves_channels_and_helpers)
{
    encodes("\x1b[38;2;0;127;255m", fg(RGB{RGB{0,127,255}}));
    encodes("\x1b[48;2;240;33;39m", bg(RGB{0xF02127}));
    CHECK(encode(fg(colors::red)) == "\x1b[31m");
    CHECK(encode(bg(RGB{0x010203})) == "\x1b[48;2;1;2;3m");
}
TEST(ENC_03_traits_enable_without_replacing_other_traits)
{
    encodes("", Traits{});
    encodes("", Trait::None);

    struct Case { Trait trait; char const* bytes; };
    Case const cases[] = {
        {Trait::Bold,"\x1b[1m"},
        {Trait::Dim,"\x1b[2m"},
        {Trait::Italic,"\x1b[3m"},
        {Trait::Underline,"\x1b[4m"},
        {Trait::Blink,"\x1b[5m"},
        {Trait::Inverse,"\x1b[7m"},
        {Trait::Invisible,"\x1b[8m"},
        {Trait::CrossedOut,"\x1b[9m"},
        {Trait::DoubleUnderline,"\x1b[21m"},
        {Trait::Standout,"\x1b[1;7m"},
    };
    for (auto const& c : cases) encodes(c.bytes,c.trait);
}
TEST(ENC_03_trait_normalization_and_double_underline_precedence)
{
    encodes("\x1b[1;7m", Trait::Standout | Trait::Bold | Trait::Inverse);
    encodes("\x1b[21m", Trait::Underline | Trait::DoubleUnderline);
    encodes("\x1b[1;2;3;21;5;7;8;9m", contract::full_traits());
}
TEST(ENC_03_style_order_and_full_reset_are_distinct)
{
    encodes("\x1b[49m\x1b[39m\x1b[22;23;24;25;27;28;29m", Style{});
    encodes("\x1b[44m\x1b[97m\x1b[22;23;24;25;27;28;29;1m",
            Style{.background=colors::blue,.foreground=colors::bright_white,.traits=Trait::Bold});
    encodes("\x1b[0m", ResetStyle{});
}
TEST(ENC_02_composition_retains_order_and_repeated_commands)
{
    encodes(""); // Zero commands.
    encodes("\x1b[31m\x1b[1;1H\x1b[31m\x1b[0m", fg(colors::red),Cursor{},fg(colors::red),ResetStyle{});
    const Cursor cursor_value{4,2};
    encodes("\x1b[3;5H\x1b[3;5H",cursor_value,cursor_value);
}
TEST(ENC_01_protocol_modes_are_explicit)
{
    encodes("\x1b[?1049h",ScreenBuffer::Alternate);
    encodes("\x1b[?1049l",ScreenBuffer::Normal);
    encodes("\x1b[?25h",CursorMode::Show);
    encodes("\x1b[?25l",CursorMode::Hide);
    encodes("\x1b[?1004h",FocusReporting::On);
    encodes("\x1b[?1004l",FocusReporting::Off);
    encodes("\x1b[?2004h",BracketedPaste::On);
    encodes("\x1b[?2004l",BracketedPaste::Off);
}
TEST(ENC_01_mouse_replacement_clears_all_owned_modes_first)
{
    constexpr auto off = "\x1b[?1000l\x1b[?1002l\x1b[?1003l\x1b[?1005l\x1b[?1006l\x1b[?1015l\x1b[?1016l";
    encodes(off,MouseReporting{MouseMode::Off,MouseProtocol::None});
    encodes(std::string{off}+"\x1b[?1006h\x1b[?1000h",MouseReporting{MouseMode::Basic,MouseProtocol::Sgr});
    encodes(std::string{off}+"\x1b[?1015h\x1b[?1002h",MouseReporting{MouseMode::Drag,MouseProtocol::Urxvt});
    encodes(std::string{off}+"\x1b[?1003h",MouseReporting{MouseMode::Move,MouseProtocol::Legacy});
    CHECK_THROWS(std::invalid_argument,encode(MouseReporting{MouseMode::Basic,MouseProtocol::None}));
}
TEST(BUF_01_late_invalid_command_never_changes_destination)
{
    std::string output{"prefix"};
    CHECK_THROWS(std::invalid_argument,append_encoded(output,Cursor{},Cursor{-1,0}));
    CHECK(output == "prefix");
    std::array<char,128> buffer;
    buffer.fill('#');
    auto const before = buffer;
    CHECK_THROWS(std::invalid_argument,encode_to(buffer,Cursor{},BlankRow{static_cast<Erase>(99)}));
    CHECK(buffer == before);
}
TEST(BUF_02_empty_span_reports_required_size_without_writing)
{
    auto result = encode_to(std::span<char>{},Cursor{});
    CHECK(result.written == 0 && result.required == 6);
    auto empty = encode_to(std::span<char>{});
    CHECK(empty.written == 0 && empty.required == 0);
}
TEST(BUF_03_owned_results_are_independent_of_later_encodings)
{
    auto first = encode(fg(colors::red));
    auto second = encode(fg(colors::blue));
    static_cast<void>(encode(Style{}));
    CHECK(first == "\x1b[31m");
    CHECK(second == "\x1b[34m");
}

TEST(ENC_04_invalid_enum_values_are_not_silently_treated_as_defaults)
{
    CHECK_THROWS(std::invalid_argument,encode(static_cast<ScreenBuffer>(99)));
    CHECK_THROWS(std::invalid_argument,encode(static_cast<CursorMode>(99)));
    CHECK_THROWS(std::invalid_argument,encode(MouseReporting{MouseMode::Off,static_cast<MouseProtocol>(99)}));
}
