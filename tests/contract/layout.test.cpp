#include <array>
#include <esc/glyph.hpp>
#include <esc/layout.hpp>
#include <esc/text.hpp>
#include <ranges>
#include <vector>
#include "data/grapheme_break_cases.hpp"
#include "support/allocations.hpp"
#include "support/test.hpp"
using namespace esc;
static_assert(std::ranges::forward_range<GraphemeView<char>>);
static_assert(std::ranges::borrowed_range<GraphemeView<char32_t>>);

TEST(TXT_LAYOUT_all_766_official_unicode17_grapheme_cases)
{
    for (auto const& item : unicode_cases::cases) {
        auto input = std::u32string_view{unicode_cases::scalars + item.scalar_offset,
                                         item.scalar_count};
        auto expected = std::span{unicode_cases::boundaries + item.boundary_offset,
                                  item.boundary_count};
        std::size_t offset = 0, index = 1;
        CHECK(expected[0] == 0);
        for (auto cluster : graphemes(input)) {
            offset += cluster.size();
            CHECK(index < expected.size());
            CHECK(offset == expected[index++]);
        }
        CHECK(index == expected.size());
        auto utf8 = utf32_to_utf8(input);
        offset = 0;
        index = 1;
        for (auto cluster : graphemes(std::string_view{utf8})) {
            offset += utf8_to_utf32(cluster).size();
            CHECK(offset == expected[index++]);
        }
        CHECK(index == expected.size());
    }
}
TEST(TXT_LAYOUT_borrowed_views_and_iterator_values)
{
    std::string_view input = "a\xcc\x81\xe4\xb8\xad";
    auto view = graphemes(input);
    auto it = view.begin();
    auto old = it++;
    CHECK(*old == input.substr(0, 3));
    CHECK((*old).data() == input.data());
    CHECK(*it == input.substr(3));
    CHECK(it != old);
    CHECK(++it == view.end());
    CHECK(next_grapheme(input) == input.substr(0, 3));
    CHECK(graphemes(std::string_view{}).begin() == std::default_sentinel);
    CHECK(next_grapheme(std::u32string_view{}).empty());
}
TEST(TXT_LAYOUT_width_goldens_for_text_and_joined_clusters)
{
    CHECK(cell_width("") == 0);
    CHECK(cell_width("Hello") == 5);
    CHECK(cell_width(U"e\u0301") == 1);
    CHECK(cell_width(U"\u00e9") == 1);
    CHECK(cell_width(U"中Ａｶ") == 5);
    CHECK(cell_width(U"\u1100\u1161\u11a8") == 2);
    CHECK(cell_width(U"\u0915\u094d\u0937") == 1);
    CHECK(cell_width(U"\U0001f469\u200d\U0001f4bb") == 2);
    CHECK(cell_width(U"\U0001f1fa\U0001f1f8") == 2);
    CHECK(cell_width(U"\U0001f44d\U0001f3fd") == 2);
    CHECK(cell_width(U"1\ufe0f\u20e3") == 2);
    CHECK(cell_width(U"1\u20e3") == 2);
    CHECK(cell_width(U"A\u20e3") == 1);
    CHECK(cell_width(U"\u2764") == 1);
    CHECK(cell_width(U"\u2764\ufe0f") == 2);
    CHECK(cell_width(U"\u2764\ufe0e") == 1);
    CHECK(cell_width(U"\U0001f3f4\U000e0067\U000e0062\U000e007f") == 2);
}
TEST(TXT_LAYOUT_terminal_policies_are_explicit)
{
    WidthPolicy p;
    CHECK(cell_width(U"·", p) == 1);
    p.ambiguous = AmbiguousWidth::Two;
    CHECK(cell_width(U"·", p) == 2);
    CHECK(cell_width("ASCII", p) == 5);
    p.emoji = EmojiWidth::One;
    CHECK(cell_width(U"\U0001f469\u200d\U0001f4bb", p) == 1);
    CHECK(cell_width(U"1\ufe0f\u20e3", p) == 1);
    p.clusters = ClusterWidth::Additive;
    CHECK(cell_width(U"\U0001f469\u200d\U0001f4bb", p) == 2);
    CHECK(cell_width(U"\u0915\u094d\u0937", p) == 2);
    p.emoji = EmojiWidth::Two;
    CHECK(cell_width(U"\U0001f44d\U0001f3fd", p) == 4);
    CHECK(cell_width(U"\u0301", p) == 0);
    p.orphan_marks = OrphanMarkWidth::One;
    CHECK(cell_width(U"\u0301\u0302", p) == 1);
    CHECK(cell_width(U"a\u0301", p) == 1);
    CHECK(cell_width(U"\u200d", p) == 0);
    p.unassigned = UnassignedWidth::Unknown;
    CHECK(!cell_width(U"\u0378", p));
    CHECK(cell_width(U"\ue000", p) == 2);  // PUA is assigned and East Asian ambiguous.
}
TEST(TXT_LAYOUT_overrides_cover_font_and_unicode_version_differences)
{
    std::array overrides = {ScalarWidthOverride{U'·', 1}, ScalarWidthOverride{0x378, 2},
                            ScalarWidthOverride{0xe000, 1},
                            ScalarWidthOverride{0x1f600, 1}};
    WidthPolicy p;
    p.overrides = overrides;
    p.unassigned = UnassignedWidth::Unknown;
    p.ambiguous = AmbiguousWidth::Two;
    CHECK(cell_width(U"·\u0378\ue000\U0001f600", p) == 5);
    std::array controls = {ScalarWidthOverride{U'\t', 0}};
    p.overrides = controls;
    CHECK(!cell_width("\t", p));  // Overrides cannot erase cursor-dependent controls.
}
TEST(TXT_LAYOUT_controls_have_no_fixed_width_and_validation_continues)
{
    for (char32_t cp = 0; cp < 32; ++cp)
        CHECK(!cell_width(std::u32string_view{&cp, 1}));
    CHECK(!cell_width(U"\u007f\u0085\u2028\u2029"));
    CHECK(!cell_width("\x1b[31mhello"));
    CHECK(!cell_width("a\r\nb"));
    CHECK(cell_width(U"\u200b\u200d\ufe0f") == 0);
    CHECK_THROWS(UnicodeError, cell_width(std::string_view{"\t\xff", 2}));
    char32_t malformed[] = {U'\n', 0xd800};
    CHECK_THROWS(UnicodeError, cell_width(std::u32string_view{malformed, 2}));
}
TEST(TXT_LAYOUT_grapheme_width_requires_one_cluster)
{
    CHECK(grapheme_width(U"") == 0);
    CHECK(grapheme_width(U"e\u0301") == 1);
    CHECK(!grapheme_width(U"\r\n"));
    CHECK_THROWS(std::invalid_argument, grapheme_width(U"ab"));
    CHECK_THROWS(std::invalid_argument, grapheme_width("ab"));
}
TEST(TXT_LAYOUT_invalid_encoding_offsets_are_in_original_units)
{
    CHECK_THROWS(UnicodeError, next_grapheme(std::string_view{"\xc0\xaf", 2}));
    CHECK_THROWS(UnicodeError, next_grapheme(std::string_view{"\xf0\x9f", 2}));
    char32_t out_of_range = 0x110000;
    CHECK_THROWS(UnicodeError, cell_width(std::u32string_view{&out_of_range, 1}));
    std::string_view input{"a\xe4\xb8\xad\xff", 5};
    try {
        for (auto cluster : graphemes(input))
            (void)cluster;
        CHECK(false);
    }
    catch (UnicodeError const& error) {
        CHECK(error.offset() == 4);
    }
    Glyph invalid_glyphs[] = {Glyph{U'a'}, Glyph{0xd800}};
    CHECK_THROWS(UnicodeError, cell_width(std::span<Glyph const>{invalid_glyphs}));
    char32_t invalid[] = {U'a', U'b', 0xd800};
    try {
        for (auto cluster : graphemes(std::u32string_view{invalid, 3}))
            (void)cluster;
        CHECK(false);
    }
    catch (UnicodeError const& error) {
        CHECK(error.offset() == 2);
    }
}
TEST(TXT_LAYOUT_invalid_policy_is_rejected_even_for_empty_input)
{
    WidthPolicy p;
    p.emoji = static_cast<EmojiWidth>(42);
    CHECK_THROWS(std::invalid_argument, cell_width("", p));
    std::array duplicate = {ScalarWidthOverride{U'a', 1}, ScalarWidthOverride{U'a', 2}};
    p = {};
    p.overrides = duplicate;
    CHECK_THROWS(std::invalid_argument, cell_width("", p));
    std::array reversed = {ScalarWidthOverride{U'z', 1}, ScalarWidthOverride{U'a', 2}};
    p.overrides = reversed;
    CHECK_THROWS(std::invalid_argument, cell_width("", p));
    std::array too_wide = {ScalarWidthOverride{U'a', 3}};
    p.overrides = too_wide;
    CHECK_THROWS(std::invalid_argument, cell_width("", p));
}
TEST(TXT_LAYOUT_utf8_and_utf32_measurements_agree)
{
    for (auto const& item : unicode_cases::cases) {
        auto input = std::u32string_view{unicode_cases::scalars + item.scalar_offset,
                                         item.scalar_count};
        auto utf8 = utf32_to_utf8(input);
        for (auto mode : {ClusterWidth::Maximum, ClusterWidth::Additive}) {
            WidthPolicy p;
            p.clusters = mode;
            CHECK(cell_width(input, p) == cell_width(utf8, p));
        }
    }
}
TEST(TXT_LAYOUT_long_contexts_are_linear_and_allocate_nothing)
{
    std::u32string text = U"\U0001f469";
    text.append(100000, U'\u0301');
    text += U"\u200d\U0001f4bb";
    auto utf8 = utf32_to_utf8(text);
    contract::AllocationScope scope{true};
    CHECK(next_grapheme(std::u32string_view{text}).size() == text.size());
    CHECK(next_grapheme(std::string_view{utf8}).size() == utf8.size());
    CHECK(cell_width(text) == 2);
    CHECK(cell_width(utf8) == 2);
    CHECK(grapheme_width(text) == 2);
    std::size_t count = 0;
    for (auto cluster : graphemes(std::string_view{utf8})) {
        (void)cluster;
        ++count;
    }
    CHECK(count == 1);
    CHECK(scope.count() == 0);
}

TEST(TXT_LAYOUT_ascii_fast_paths_preserve_unicode_boundaries_and_policy)
{
    CHECK(cell_width(U"abc1\ufe0f\u20e3") == 5);
    CHECK(cell_width(U"abcA\u0301") == 4);
    CHECK(cell_width(U"ab\u0600cd") == 4);
    CHECK(!cell_width(std::string_view{"abc\0xyz", 7}));
    CHECK_THROWS(UnicodeError, cell_width(std::string_view{"abc\xff", 4}));
    CHECK_THROWS(std::invalid_argument, grapheme_width("abcdef"));
    std::array entries = {ScalarWidthOverride{U'a', 2}};
    WidthPolicy p;
    p.overrides = entries;
    CHECK(cell_width("aaaa", p) == 8);
    CHECK(next_grapheme(std::string_view{"a\xcc\x81"}) == "a\xcc\x81");
    auto range = graphemes(std::string_view{"ab1\xef\xb8\x8f\xe2\x83\xa3"});
    CHECK(std::ranges::distance(range) == 3);
}

TEST(TXT_LAYOUT_styled_glyphs_are_measured_without_copying_or_losing_brushes)
{
    auto styled = glyphs(U"e\u0301中");
    styled[1].brush.traits = Trait::Bold;
    contract::AllocationScope scope{true};
    auto first = next_grapheme(std::span<Glyph const>{styled});
    CHECK(first.size() == 2);
    CHECK(first.data() == styled.data());
    CHECK(first[1].brush.traits == Trait::Bold);
    CHECK(cell_width(styled) == 3);
    CHECK(grapheme_width(first) == 1);
    CHECK(cell_width(std::span<Glyph const>{}) == 0);
    CHECK(scope.count() == 0);
}
