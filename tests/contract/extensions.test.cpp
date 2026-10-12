#include <array>
#include <climits>
#include <esc/esc.hpp>
#include "support/allocations.hpp"
#include "support/test.hpp"
using namespace esc;
namespace {
auto extensions_profile() -> TerminalProfile
{
    TerminalProfile p;
    p.control = true;
    p.cursor_style = true;
    p.cursor_save_restore = true;
    p.scrolling = true;
    p.editing = true;
    p.window_title = true;
    p.hyperlinks = true;
    p.synchronized_output = true;
    return p;
}
}  // namespace
TEST(ENC_EXT_cursor_styles_have_literal_goldens)
{
    std::string_view expected[] = {"\x1b[0 q", "\x1b[1 q", "\x1b[2 q", "\x1b[3 q",
                                   "\x1b[4 q", "\x1b[5 q", "\x1b[6 q"};
    for (unsigned i = 0; i < 7; ++i)
        CHECK(encode(static_cast<CursorStyle>(i)) == expected[i]);
    CHECK(encode(SaveCursor{}, RestoreCursor{}) ==
          "\x1b"
          "7\x1b"
          "8");
    CHECK_THROWS(std::invalid_argument, encode(static_cast<CursorStyle>(7)));
}
TEST(ENC_EXT_scroll_region_uses_inclusive_zero_based_rows)
{
    CHECK(encode(ScrollRegion{2, 9}, ResetScrollRegion{}) == "\x1b[3;10r\x1b[r");
    CHECK(encode(ScrollRegion{0, INT_MAX}) == "\x1b[1;2147483648r");
    CHECK_THROWS(std::invalid_argument, encode(ScrollRegion{-1, 2}));
    CHECK_THROWS(std::invalid_argument, encode(ScrollRegion{2, 2}));
    CHECK_THROWS(std::invalid_argument, encode(ScrollRegion{3, 2}));
}
TEST(ENC_EXT_scroll_and_edit_counts_do_not_invent_zero_as_one)
{
    CHECK(encode(ScrollUp{2}, ScrollDown{3}, InsertCharacters{4}, DeleteCharacters{5},
                 InsertLines{6},
                 DeleteLines{7}) == "\x1b[2S\x1b[3T\x1b[4@\x1b[5P\x1b[6L\x1b[7M");
    CHECK(encode(ScrollUp{0}, ScrollDown{0}, InsertCharacters{0}, DeleteCharacters{0},
                 InsertLines{0}, DeleteLines{0})
              .empty());
    CHECK(encode(InsertCharacters{UINT_MAX}) == "\x1b[4294967295@");
}
TEST(ENC_EXT_osc_literals_preserve_utf8_and_use_ST)
{
    CHECK(encode(WindowTitle{"Hello 界"}) == "\x1b]2;Hello 界\x1b\\");
    CHECK(encode(WindowTitle{""}) == "\x1b]2;\x1b\\");
    CHECK(encode(BeginHyperlink{"https://example.com/a;b", "row-1"}, EndHyperlink{}) ==
          "\x1b]8;id=row-1;https://example.com/a;b\x1b\\\x1b]8;;\x1b\\");
    CHECK(encode(BeginHyperlink{"file:///tmp/界"}) == "\x1b]8;;file:///tmp/界\x1b\\");
    CHECK(encode(SynchronizedOutput::On, SynchronizedOutput::Off) ==
          "\x1b[?2026h\x1b[?2026l");
}
TEST(ENC_EXT_payload_controls_and_invalid_unicode_are_rejected)
{
    for (unsigned c = 0; c < 32; ++c) {
        std::string text(1, static_cast<char>(c));
        CHECK_THROWS(std::invalid_argument, encode(WindowTitle{text}));
        CHECK_THROWS(std::invalid_argument, encode(BeginHyperlink{text}));
    }
    for (char32_t c = 0x7f; c <= 0x9f; ++c) {
        auto text = utf32_to_utf8(std::u32string_view{&c, 1});
        CHECK_THROWS(std::invalid_argument, encode(WindowTitle{text}));
        CHECK_THROWS(std::invalid_argument, encode(BeginHyperlink{text}));
    }
    CHECK_THROWS(std::invalid_argument, encode(WindowTitle{"\xc2\x9c"}));
    CHECK_THROWS(UnicodeError, encode(WindowTitle{"\xff"}));
    CHECK_THROWS(UnicodeError, encode(BeginHyperlink{"\xed\xa0\x80"}));
    CHECK_THROWS(std::invalid_argument, encode(BeginHyperlink{""}));
    for (auto id : {"a:b", "a;b", "a=b", "a b", "界"})
        CHECK_THROWS(std::invalid_argument,
                     encode(BeginHyperlink{"https://example.com", id}));
}
TEST(BUF_EXT_bounded_and_reserved_osc_encoding_allocate_nothing)
{
    std::array<char, 256> storage;
    storage.fill('!');
    auto expected = encode(WindowTitle{"界"},
                           BeginHyperlink{"https://example.com", "x"}, EndHyperlink{});
    std::string output = "prefix";
    output.reserve(256);
    contract::AllocationScope scope{true};
    auto result = encode_to(storage, WindowTitle{"界"},
                            BeginHyperlink{"https://example.com", "x"}, EndHyperlink{});
    CHECK(result.written == expected.size());
    CHECK(std::string_view{storage.data(), result.written} == expected);
    append_encoded(output, WindowTitle{"界"}, BeginHyperlink{"https://example.com", "x"},
                  EndHyperlink{});
    CHECK(std::string_view{output}.substr(6) == expected);
    CHECK(scope.count() == 0);
}
TEST(BUF_EXT_short_capacity_and_invalid_later_command_preserve_storage)
{
    std::array<char, 32> storage;
    storage.fill('!');
    auto before = storage;
    auto result = encode_to(std::span<char>{storage}.first(3), WindowTitle{"hello"});
    CHECK(result.written == 0 && result.required == 11);
    CHECK(storage == before);
    CHECK_THROWS(std::invalid_argument,
                 encode_to(storage, WindowTitle{"ok"}, ScrollRegion{2, 1}));
    CHECK(storage == before);
    std::string output = "prefix";
    CHECK_THROWS(UnicodeError,
                 append_encoded(output, SaveCursor{}, WindowTitle{"\xff"}));
    CHECK(output == "prefix");
}
TEST(BUF_EXT_append_snapshots_aliased_payloads_before_growth)
{
    std::string output = "https://example.com";
    output.shrink_to_fit();
    auto original = output;
    append_encoded(output, WindowTitle{output}, BeginHyperlink{output}, EndHyperlink{});
    CHECK(output == original + encode(WindowTitle{original}, BeginHyperlink{original},
                                      EndHyperlink{}));
    output = "https://example.com";
    original = output;
    append_encoded(output, extensions_profile(), WindowTitle{output});
    CHECK(output == original + encode(WindowTitle{original}));
    output = "abcdefghijklmnopqrstuvwxyz";
    auto before = output;
    output.shrink_to_fit();
    {
        contract::AllocationScope reject{true};
        CHECK_THROWS(std::bad_alloc, append_encoded(output, WindowTitle{output}));
    }
    CHECK(output == before);
}
TEST(BUF_EXT_span_encoding_rejects_overlapping_payloads)
{
    std::array<char, 128> storage{};
    std::copy_n("title", 5, storage.begin());
    auto before = storage;
    CHECK_THROWS(std::invalid_argument,
                 encode_to(storage, WindowTitle{std::string_view{storage.data(), 5}}));
    CHECK(storage == before);
    CHECK_THROWS(std::invalid_argument,
                 encode_to(storage, extensions_profile(),
                           BeginHyperlink{std::string_view{storage.data(), 5}}));
    CHECK(storage == before);
    std::copy_n("title", 5, storage.begin() + 80);
    auto result = encode_to(std::span<char>{storage}.first(11),
                            WindowTitle{std::string_view{storage.data() + 80, 5}});
    CHECK(result.written == 11 && result.required == 11);
    CHECK(std::string_view{storage.data(), 11} == "\x1b]2;title\x1b\\");
}
TEST(CAP_EXT_optional_extensions_are_explicit_and_validate_before_fallback)
{
    TerminalProfile plain;
    CHECK(encode(plain, CursorStyle::SteadyBar, SaveCursor{}, ScrollUp{}, InsertLines{},
                 WindowTitle{"x"}, BeginHyperlink{"https://example.com"},
                 EndHyperlink{}, SynchronizedOutput::On)
              .empty());
    plain.fallback = FallbackPolicy::Strict;
    CHECK(encode(plain, ScrollUp{0}, InsertCharacters{0}).empty());
    CHECK_THROWS(UnsupportedFeature, encode(plain, WindowTitle{"x"}));
    CHECK_THROWS(std::invalid_argument, encode(TerminalProfile{}, WindowTitle{"\x1b"}));
    CHECK_THROWS(UnicodeError, encode(TerminalProfile{}, BeginHyperlink{"\xff"}));
    auto full = extensions_profile();
    CHECK(encode(full, ScrollUp{}, WindowTitle{"x"}) ==
          encode(ScrollUp{}, WindowTitle{"x"}));
    full.control = false;
    CHECK_THROWS(std::invalid_argument, encode(full, WindowTitle{"x"}));
}
TEST(CAP_EXT_report_lists_each_new_feature_without_environment_guesses)
{
    auto report = resolve_profile(Environment{"xterm-256color", "", false}, true);
    Feature expected[] = {Feature::CursorStyle,       Feature::CursorSaveRestore,
                          Feature::Scrolling,         Feature::Editing,
                          Feature::WindowTitle,       Feature::Hyperlinks,
                          Feature::SynchronizedOutput};
    CHECK(report.decisions.size() == 15);
    for (std::size_t i = 0; i < 7; ++i) {
        CHECK(report.decisions[i + 8].feature == expected[i]);
        CHECK(report.decisions[i + 8].source == EvidenceSource::Default);
    }
    CHECK(!report.profile.window_title && !report.profile.hyperlinks &&
          !report.profile.synchronized_output);
}

TEST(ENC_EXT_ascii_payload_scanning_preserves_validation_at_every_word_offset)
{
    for (std::size_t offset = 0; offset < 16; ++offset) {
        std::string payload(40, 't');
        for (unsigned byte = 0; byte < 256; ++byte) {
            payload[offset + 8] = static_cast<char>(byte);
            if (byte >= 128) {
                try {
                    static_cast<void>(encode(WindowTitle{payload}));
                    CHECK(false);
                }
                catch (UnicodeError const& error) { CHECK(error.offset() == offset + 8); }
            }
            else if (byte < 32 || byte == 127)
                CHECK_THROWS(std::invalid_argument, encode(WindowTitle{payload}));
            else
                CHECK(encode(WindowTitle{payload}) == "\x1b]2;" + payload + "\x1b\\");
        }
        payload = std::string(offset, 't') + "界\xf0\x9f\x91\xbd";
        CHECK(encode(WindowTitle{payload}) == "\x1b]2;" + payload + "\x1b\\");
        payload += "\xc2\x9f";
        std::string destination = "prefix";
        CHECK_THROWS(std::invalid_argument, append_encoded(destination, WindowTitle{payload}));
        CHECK(destination == "prefix");
    }
}
