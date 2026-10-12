#include "support/encoding.hpp"
#include <esc/text.hpp>
#include <esc/glyph.hpp>

#include <iterator>
#include <list>
#include <type_traits>

using namespace esc;
using namespace std::literals;

static_assert(std::same_as<GlyphString,std::vector<Glyph>>);
static_assert(GlyphRange<GlyphString> && GlyphRange<std::list<Glyph>>);
static_assert(!std::convertible_to<Utf8CodePoint,std::string_view>);
template<class T> concept ScalarPipe = requires(T value) { value | Trait::Bold; };
static_assert(!ScalarPipe<char8_t> && !ScalarPipe<char16_t>);

TEST(TXT_01_unicode_boundary_goldens_round_trip)
{
    struct Case { char32_t scalar; std::string_view utf8; };
    Case const cases[] = {
        {U'\0',"\0"sv},{U'A',"A"sv},{0x7F,"\x7f"sv},
        {0x80,"\xc2\x80"sv},{0x7FF,"\xdf\xbf"sv},
        {0x800,"\xe0\xa0\x80"sv},{0xD7FF,"\xed\x9f\xbf"sv},
        {0xE000,"\xee\x80\x80"sv},{0xFFFF,"\xef\xbf\xbf"sv},
        {0x10000,"\xf0\x90\x80\x80"sv},{0x10FFFF,"\xf4\x8f\xbf\xbf"sv},
    };
    for (auto const& c : cases) {
        auto encoded=encode_utf8(c.scalar);
        CHECK(encoded.view() == c.utf8);
        CHECK(encoded.size == c.utf8.size());
        for (auto i=encoded.size;i<encoded.bytes.size();++i) CHECK(encoded.bytes[i]==0);
        CHECK(utf32_to_utf8(std::u32string{c.scalar}) == c.utf8);
        CHECK(utf8_to_utf32(c.utf8) == std::u32string{c.scalar});
    }
}
TEST(TXT_01_empty_text_nulls_and_noncharacters_are_preserved)
{
    CHECK(utf8_to_utf32("").empty());
    CHECK(utf32_to_utf8(U"").empty());
    CHECK(utf8_to_utf32("a\0b"sv) == U"a\0b"sv);
    CHECK(utf32_to_utf8(U"a\0b"sv) == "a\0b"sv);
    CHECK(utf8_to_utf32("\xef\xb7\x90"sv) == std::u32string{0xFDD0});
}
TEST(TXT_01_invalid_utf8_has_a_precise_error_offset)
{
    std::string_view const invalid[] = {
        "\x80"sv,"\xbf"sv,"\xc0\xaf"sv,"\xc1\xbf"sv,
        "\xe0\x80\xaf"sv,"\xf0\x80\x80\xaf"sv,
        "\xed\xa0\x80"sv,"\xed\xbf\xbf"sv,
        "\xf4\x90\x80\x80"sv,"\xf5\x80\x80\x80"sv,"\xff"sv,
        "\xc2"sv,"\xe2\x82"sv,"\xf0\x9f\x91"sv,"\xe2(\xa1"sv,
    };
    for (auto bytes : invalid) {
        CHECK_THROWS(UnicodeError,utf8_to_utf32(bytes));
        try {
            static_cast<void>(utf8_to_utf32("ok"+std::string{bytes}));
            CHECK(false);
        } catch (UnicodeError const& error) {
            CHECK(error.offset()==2);
        }
    }
}
TEST(TXT_01_invalid_utf32_and_single_scalars_are_rejected)
{
    for (char32_t invalid : {char32_t{0xD800},char32_t{0xDFFF},char32_t{0x110000},char32_t{0xFFFFFFFF}}) {
        CHECK_THROWS(UnicodeError,encode_utf8(invalid));
        try {
            static_cast<void>(utf32_to_utf8(std::u32string{U'a',invalid}));
            CHECK(false);
        } catch (UnicodeError const& error) { CHECK(error.offset()==1); }
    }
}
TEST(TXT_02_append_preserves_prefix_and_is_transactional)
{
    std::string utf8="prefix";
    append_utf8(utf8,U"界👽");
    CHECK(utf8=="prefix\xe7\x95\x8c\xf0\x9f\x91\xbd");
    auto before8=utf8;
    CHECK_THROWS(UnicodeError,append_utf8(utf8,std::u32string{U'a',0xD800}));
    CHECK(utf8==before8);
    std::u32string utf32=U"prefix";
    append_utf32(utf32,"\xe7\x95\x8c\xf0\x9f\x91\xbd"sv);
    CHECK(utf32==U"prefix界👽");
    auto before32=utf32;
    CHECK_THROWS(UnicodeError,append_utf32(utf32,"valid\xc2"sv));
    CHECK(utf32==before32);
    append_utf8(utf8,U"");
    append_utf32(utf32,"");
    CHECK(utf8==before8 && utf32==before32);
}
TEST(BUF_03_single_codepoint_values_own_their_bytes)
{
    auto first=encode_utf8(U'界');
    auto second=encode_utf8(U'👽');
    CHECK(first.view()=="\xe7\x95\x8c"sv);
    CHECK(second.view()=="\xf0\x9f\x91\xbd"sv);
    auto copy=first;
    first.bytes[0]='x';
    CHECK(copy.view()=="\xe7\x95\x8c"sv);
}
TEST(GLY_01_glyphs_represent_scalars_not_screen_cells)
{
    auto result=glyphs("e\xcc\x81\xe7\x95\x8c\xf0\x9f\x91\xbd"sv);
    CHECK(result==GlyphString{{U'e'},{0x301},{U'界'},{U'👽'}});
    CHECK(glyphs("a\0b"sv)==GlyphString{{U'a'},{U'\0'},{U'b'}});
    CHECK(glyphs(U"a\0b"sv)==GlyphString{{U'a'},{U'\0'},{U'b'}});
    CHECK(glyphs("").empty() && glyphs(U"").empty());
}
TEST(GLY_01_glyph_construction_and_append_use_the_supplied_brush)
{
    Style style{.foreground=XColor::Red,.traits=Trait::Bold};
    CHECK(glyphs("ab",style)==GlyphString{{U'a',style},{U'b',style}});
    GlyphString result{{U'x'}};
    append_glyphs(result,U"界",style);
    append_glyphs(result,"\xf0\x9f\x91\xbd"sv,style);
    CHECK(result==GlyphString{{U'x'},{U'界',style},{U'👽',style}});
    auto before=result;
    CHECK_THROWS(UnicodeError,append_glyphs(result,"ok\xc2"sv,style));
    CHECK_THROWS(UnicodeError,append_glyphs(result,std::u32string{U'a',0xD800},style));
    CHECK(result==before);
}
TEST(GLY_01_glyph_pipes_and_style_replacement_are_pure)
{
    Glyph original{U'x',Style{.foreground=XColor::Red,.traits=Trait::Bold}};
    auto changed=original | bg(XColor::Blue) | Trait::Italic;
    CHECK(original.style.background==Color{TermColor::Default});
    CHECK(original.style.traits==Traits{Trait::Bold});
    CHECK(changed.style.background==Color{XColor::Blue});
    CHECK(changed.style.foreground==Color{XColor::Red});
    CHECK(changed.style.traits==(Trait::Bold | Trait::Italic));
    auto removed=changed | without(Trait::Bold);
    CHECK(removed.style.traits==Traits{Trait::Italic});
    CHECK((changed | Style{})==Glyph{U'x'});
}
TEST(GLY_01_lvalue_and_const_ranges_are_not_modified_by_styling)
{
    GlyphString input{{U'a'},{U'b'}};
    auto expected=input;
    auto piped=input | fg(XColor::Red);
    CHECK(input==expected);
    CHECK(piped==glyphs("ab",Style{.foreground=XColor::Red}));
    GlyphString const immutable=input;
    CHECK(styled(immutable,Trait::Bold)==glyphs("ab",Style{.traits=Trait::Bold}));
    CHECK(immutable==expected);
    auto moved=std::move(piped) | Trait::Italic;
    CHECK(moved==glyphs("ab",Style{.foreground=XColor::Red,.traits=Trait::Italic}));
}
TEST(GLY_01_named_in_place_styling_supports_mutable_ranges)
{
    GlyphString input{{U'a'},{U'b'}};
    style_in_place(input,Trait::Bold | Trait::Italic);
    CHECK(input==glyphs("ab",Style{.traits=Trait::Bold | Trait::Italic}));
    style_in_place(input,without(Trait::Bold));
    CHECK(input==glyphs("ab",Style{.traits=Trait::Italic}));
    std::list<Glyph> list{{U'a'},{U'b'}};
    style_in_place(list,bg(XColor::Blue));
    CHECK(styled(list,fg(XColor::Red))==glyphs("ab",Style{.background=XColor::Blue,.foreground=XColor::Red}));
}
TEST(GLY_01_ascii_and_scalar_pipes_have_explicit_encoding_rules)
{
    CHECK(('A' | fg(XColor::Red))==Glyph{U'A',Style{.foreground=XColor::Red}});
    CHECK((U'界' | Trait::Bold)==Glyph{U'界',Style{.traits=Trait::Bold}});
    CHECK_THROWS(std::invalid_argument,(static_cast<unsigned char>(128) | Trait::Bold));
    CHECK_THROWS(std::invalid_argument,(static_cast<signed char>(-1) | Trait::Bold));
    CHECK(("\xe7\x95\x8c"sv | Trait::Bold).content == "界");
}
TEST(GLY_01_concatenation_preserves_order_and_inputs)
{
    Glyph a{U'a',Style{.foreground=XColor::Red}};
    Glyph b{U'b'};
    CHECK((a+b)==GlyphString{a,b});
    CHECK((a+"界"sv)==GlyphString{a,Glyph{U'界'}});
    CHECK((U"界"sv+a)==GlyphString{Glyph{U'界'},a});
    GlyphString sequence{a,b};
    auto before=sequence;
    CHECK((sequence+U'界')==GlyphString{a,b,Glyph{U'界'}});
    CHECK(("x"sv+sequence)==GlyphString{Glyph{U'x'},a,b});
    CHECK(sequence==before);
    CHECK(&(sequence+=U'界')==&sequence);
    CHECK(sequence==GlyphString{a,b,Glyph{U'界'}});
}
TEST(GLY_01_self_and_overlapping_append_duplicate_original_values_once)
{
    GlyphString values{{U'a'},{U'b'},{U'c'}};
    values.shrink_to_fit();
    values+=values;
    CHECK(values==glyphs("abcabc"));
    std::span<Glyph const> suffix{values.data()+1,2};
    values+=suffix;
    CHECK(values==glyphs("abcabcbc"));
}
TEST(GLY_01_iterator_append_returns_the_advanced_iterator)
{
    std::array<Glyph,3> output;
    auto end=append_glyphs_to(output.begin(),"a界👽"sv,Style{.foreground=XColor::Red});
    CHECK(end==output.end());
    CHECK(GlyphString(output.begin(),output.end())==glyphs(U"a界👽",Style{.foreground=XColor::Red}));
    auto before=output;
    CHECK_THROWS(UnicodeError,append_glyphs_to(output.begin(),"a\xc2"sv));
    CHECK(output==before);
}

TEST(GLY_01_invalid_late_scalar_is_rejected_before_in_place_styling)
{
    GlyphString input{{U'a'},{char32_t{0xD800}}};
    auto before=input;
    CHECK_THROWS(UnicodeError,style_in_place(input,Trait::Bold));
    CHECK(input==before);
}
TEST(GLY_01_attribute_none_does_not_clear_existing_glyph_traits)
{
    Glyph original{U'a',Style{.traits=Trait::Bold}};
    CHECK((original | Trait::None)==original);
    CHECK((original | Traits{})==original);
    CHECK((original | without(Trait::Standout)).style.traits==Traits{});
}

TEST(TXT_01_error_offsets_count_bytes_after_valid_multibyte_prefixes)
{
    try {
        static_cast<void>(utf8_to_utf32("界👽\xe2(\xa1"sv));
        CHECK(false);
    } catch (UnicodeError const& error) { CHECK(error.offset()==7); }
    for (auto bytes : {"\xc2" "A"sv,"\xe2\x82" "A"sv,"\xf0\x9f\x91" "A"sv}) {
        CHECK_THROWS(UnicodeError,utf8_to_utf32(bytes));
    }
}
TEST(GLY_01_invalid_attributes_are_rejected_before_any_mutation)
{
    GlyphString values=glyphs("ab");
    auto const before=values;
    auto invalid=fg(static_cast<TermColor>(true));
    CHECK_THROWS(std::invalid_argument,style_in_place(values,invalid));
    CHECK_THROWS(std::invalid_argument,styled(values,invalid));
    CHECK_THROWS(std::invalid_argument,append_glyphs(values,"",Style{.foreground=static_cast<TermColor>(true)}));
    CHECK(values==before);
    std::array<Glyph,2> output;
    auto original=output;
    CHECK_THROWS(std::invalid_argument,append_glyphs_to(output.begin(),"ab",Style{.foreground=static_cast<TermColor>(true)}));
    CHECK(output==original);
}
TEST(GLY_01_noncontiguous_aliases_are_snapshotted_before_append)
{
    GlyphString values=glyphs("abc");
    values.shrink_to_fit();
    auto backwards=std::views::reverse(values);
    values+=backwards;
    CHECK(values==glyphs("abccba"));
    auto projected=values | std::views::transform([](Glyph glyph) { return glyph; });
    auto before=values;
    values+=projected;
    CHECK(values==before+before);
}
TEST(GLY_01_utf32_view_of_existing_symbol_survives_reallocation)
{
    GlyphString values{{U'界'}};
    values.shrink_to_fit();
    auto symbol=std::u32string_view{&values[0].symbol,1};
    append_glyphs(values,symbol);
    CHECK(values==glyphs(U"界界"));
}
TEST(GLY_01_invalid_append_sources_preserve_the_destination)
{
    GlyphString values=glyphs("prefix");
    auto const before=values;
    GlyphString invalid{{U'a'},{char32_t{0xD800}}};
    CHECK_THROWS(UnicodeError,(values+=invalid));
    CHECK_THROWS(UnicodeError,(values+=Glyph{char32_t{0x110000}}));
    CHECK_THROWS(std::invalid_argument,(values+=static_cast<unsigned char>(128)));
    CHECK(values==before);
}
TEST(GLY_01_utf32_iterator_append_and_standard_back_inserters_work)
{
    std::array<Glyph,3> output;
    CHECK(append_glyphs_to(output.begin(),U"a界👽"sv)==output.end());
    CHECK(GlyphString(output.begin(),output.end())==glyphs("a界👽"));
    std::list<Glyph> list;
    append_glyphs_to(std::back_inserter(list),"a界👽"sv);
    CHECK(styled(list,Traits{})==glyphs("a界👽"));
    auto before=output;
    CHECK_THROWS(UnicodeError,append_glyphs_to(output.begin(),std::u32string{U'a',char32_t{0xDFFF}}));
    CHECK(output==before);
}

template<class T> concept GlyphAppendDestination = requires(T& value) { value+=Glyph{}; };
static_assert(!GlyphAppendDestination<std::list<Glyph>>);
template<class T> concept MutableStyle = requires(T& value) { style_in_place(value,Trait::Bold); };
static_assert(!MutableStyle<GlyphString const>);
static_assert(encode_utf8(U'👽').size==4);

TEST(TXT_02_borrowed_object_bytes_are_preserved_even_when_the_terminator_is_overwritten)
{
    std::u32string output=U"A";
    output.reserve(32);
    // char views may legally inspect the UTF-32 object's representation, including
    // its terminator. Appending must not change the borrowed bytes mid-conversion.
    std::string_view borrowed{reinterpret_cast<char const*>(output.data()),
                              (output.size()+1)*sizeof(char32_t)};
    auto const expected=output+utf8_to_utf32(borrowed);
    append_utf32(output,borrowed);
    CHECK(output==expected);
}
