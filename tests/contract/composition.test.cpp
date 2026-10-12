#include <esc/styled_text.hpp>
#include "support/allocations.hpp"
#include "support/encoding.hpp"
#include "support/fixtures.hpp"

using namespace esc;
using namespace std::literals;

static_assert(Encodable<StylePatch> && Encodable<StyledText<std::string>>);
static_assert(Encodable<GlyphString> && Drawable<GlyphString>);
static_assert(!Drawable<decltype(sequence(ScreenBuffer::Alternate))>);
static_assert(!Drawable<decltype(styled(sequence(CursorMode::Hide), bold))>);
static_assert(!Encodable<int> && !Encodable<bool>);
constexpr auto warning_patch = fg(colors::yellow) | bold;
static_assert((Style{} | warning_patch).traits == Traits{bold});

TEST(ENC_05_mixed_commands_text_and_unicode_share_every_sink)
{
    contract::encodes("\x1b[3;5H\x1b[36m\x1b[1mHello 👽\0界\x1b[0m"sv, Cursor{4, 2},
                      fg(colors::cyan), bold, "Hello 👽\0"sv, U'界', ResetStyle{});
    contract::encodes("ab界", sequence("a", 'b', U'界'));
}

TEST(ENC_05_style_patches_are_pure_and_last_component_wins)
{
    auto original = warning_patch;
    auto changed = original | fg(colors::red) | without(bold) | underline;
    CHECK(original == warning_patch);
    CHECK((Style{.traits = bold | italic} | changed) ==
          Style{.foreground = colors::red, .traits = italic | underline});
    CHECK((changed | Style{}) == Style{});
    CHECK((Style{.foreground = colors::red} | fg(default_color)) == Style{});
    contract::encodes("\x1b[31m\x1b[22m\x1b[4m", changed);
    contract::encodes("\x1b[1m\x1b[4m", bold, underline);
    contract::encodes("", Traits{}, Trait::None, StylePatch{});
    contract::encodes("\x1b[1;2m\x1b[22m\x1b[2m", bold | dim, without(bold));
    contract::encodes("\x1b[21m\x1b[24m\x1b[4m", underline | double_underline,
                      without(double_underline));
}

TEST(ENC_05_styled_text_owns_storage_and_pipes_preserve_inputs)
{
    std::string source(128, 'a');
    auto label = styled(source, warning_patch);
    auto changed = label | underline;
    source.assign(128, 'b');
    CHECK(label.content == std::string(128, 'a'));
    CHECK((Style{} | label.appearance).traits == Traits{bold});
    CHECK((Style{} | changed.appearance).traits == (bold | underline));
    auto borrowed = styled_view(std::string_view{source}, bold);
    source[0] = 'c';
    CHECK(borrowed.content.front() == 'c');
    auto owned_sequence = sequence(std::string_view{source}, styled("ok", bold));
    source[0] = 'd';
    CHECK(std::get<0>(owned_sequence.values).front() == 'c');
    CHECK_THROWS(UnicodeError, styled("\xff", bold));
    CHECK_THROWS(std::invalid_argument,
                 styled(static_cast<char const*>(nullptr), bold));
}

TEST(ENC_05_nested_styled_runs_restore_known_enclosing_style)
{
    auto inner = styled("b", without(bold) | fg(colors::red));
    auto outer = styled(sequence("a", inner, "c"), warning_patch);
    contract::encodes(
        "\x1b[49m\x1b[33m\x1b[22;23;24;25;27;28;29;1ma"
        "\x1b[49m\x1b[31m\x1b[22;23;24;25;27;28;29mb"
        "\x1b[49m\x1b[33m\x1b[22;23;24;25;27;28;29;1mc"
        "\x1b[49m\x1b[39m\x1b[22;23;24;25;27;28;29m",
        outer);
    contract::encodes(
        "\x1b[34m\x1b[49m\x1b[34m\x1b[22;23;24;25;27;28;29;1mx"
        "\x1b[49m\x1b[34m\x1b[22;23;24;25;27;28;29my",
        fg(colors::blue), styled("x", bold), "y");
}

TEST(BUF_05_nested_borrowed_aliases_survive_append_growth)
{
    std::string output(256, 'a');
    output.shrink_to_fit();
    auto borrowed = styled_view(std::string_view{output}, bold);
    auto expected = output + "\x1b[49m\x1b[39m\x1b[22;23;24;25;27;28;29;1m" + output +
                    "\x1b[49m\x1b[39m\x1b[22;23;24;25;27;28;29m";
    append_encoded(output, sequence(), borrowed);
    CHECK(output == expected);
    std::array<char, 512> bytes;
    bytes.fill('a');
    auto label = styled_view(std::string_view{bytes.data(), 3}, bold);
    auto before = bytes;
    CHECK_THROWS(std::invalid_argument, encode_to(bytes, label));
    CHECK(bytes == before);
}

TEST(BUF_05_late_invalid_nested_values_preserve_destinations)
{
    auto bad = sequence(Cursor{}, styled("ok", bold), std::u32string{U'a', 0xd800});
    std::string output = "prefix";
    std::array<char, 256> bytes;
    bytes.fill('#');
    auto before = bytes;
    CHECK_THROWS(UnicodeError, append_encoded(output, bad));
    CHECK(output == "prefix");
    CHECK_THROWS(UnicodeError, encode_to(bytes, bad));
    CHECK(bytes == before);
    auto invalid = StylePatch{.enabled = bold, .disabled = bold};
    CHECK_THROWS(std::invalid_argument, encode(invalid));
}

TEST(BUF_05_borrowed_styled_runs_and_mixed_buffers_allocate_nothing)
{
    auto label = styled_view("Hello 👽", warning_patch);
    std::array<char, 512> bytes;
    std::string output;
    output.reserve(1024);
    std::size_t allocations;
    {
        contract::AllocationScope scope;
        auto result = encode_to(bytes, Cursor{}, label);
        CHECK(result.written > 0);
        append_encoded(output, Cursor{}, label);
        allocations = scope.count();
    }
    CHECK(allocations == 0);
}

TEST(CAP_05_styled_runs_obey_color_and_trait_fallback)
{
    auto profile = contract::full_profile();
    profile.colors = ColorSupport::Basic8;
    profile.traits = bold;
    auto label = styled("x", fg(RGB{0x800000}) | bold | italic);
    contract::encodes(
        "\x1b[49m\x1b[31m\x1b[22;23;24;25;27;28;29;1mx"
        "\x1b[49m\x1b[39m\x1b[22;23;24;25;27;28;29m",
        profile, label);
    profile.fallback = FallbackPolicy::Strict;
    CHECK_THROWS(UnsupportedFeature, encode(profile, label));
    contract::encodes("x", TerminalProfile{}, label);
}

TEST(PAR_05_result_alternatives_and_keyboard_matching_are_explicit)
{
    ReadResult value = Event{KeyEvent{U'q'}};
    auto key = std::get<KeyEvent>(std::get<Event>(value));
    CHECK(key.matches(U'q'));
    CHECK(!key.matches(KeyCode::Escape));
    key.action = KeyAction::Release;
    CHECK(!key.matches(U'q'));
    CHECK(key.matches(U'q', {}, KeyAction::Release));
    key.key.reset();
    CHECK(!key.matches(U'q', {}, KeyAction::Release));
    auto choose = [](ReadResult const& result) {
        return match(
            result, [](Event const&) { return 1; }, [](Timeout) { return 2; },
            [](EndOfInput) { return 3; });
    };
    CHECK(choose(value) == 1 && choose(Timeout{}) == 2 && choose(EndOfInput{}) == 3);
    QueryResult query = Rejected{};
    CHECK(std::holds_alternative<Rejected>(query));
    Parser parser;
    auto result = parser.next(std::span<char const>{"\x1b[A", 3});
    CHECK(result.event == std::optional<Event>{KeyEvent{KeyCode::ArrowUp}});
    CHECK(result.consumed == 3);
    CHECK(!parser.next({}).event);
    std::variant<int, std::string> reference = 2;
    match(reference, [](int& number) { ++number; }, [](std::string&) {});
    CHECK(std::get<int>(reference) == 3);
}

TEST(BUF_05_lazy_glyph_views_can_read_the_append_destination)
{
    std::string output = "abc";
    output.shrink_to_fit();
    auto projected = output | std::views::transform([](char byte) {
                         return Glyph{static_cast<char32_t>(byte)};
                     });
    append_encoded(output, projected);
    CHECK(output ==
          "abc\x1b[49m\x1b[39m\x1b[22;23;24;25;27;28;29mabc"
          "\x1b[49m\x1b[39m\x1b[22;23;24;25;27;28;29m");
}

TEST(ENC_05_glyph_values_and_ranges_share_bounded_and_owned_encoding)
{
    auto values = glyphs("a界", Style{.foreground = colors::red});
    contract::encodes(
        "\x1b[49m\x1b[31m\x1b[22;23;24;25;27;28;29ma界"
        "\x1b[49m\x1b[39m\x1b[22;23;24;25;27;28;29m",
        values);
    contract::encodes("", GlyphString{});
    contract::encodes(
        "\x1b[49m\x1b[39m\x1b[22;23;24;25;27;28;29m👽"
        "\x1b[49m\x1b[39m\x1b[22;23;24;25;27;28;29m",
        Glyph{U'👽'});
}
