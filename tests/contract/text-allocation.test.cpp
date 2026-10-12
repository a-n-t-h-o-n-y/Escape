#include "support/encoding.hpp"
#include "support/allocations.hpp"
#include <esc/text.hpp>
#include <esc/glyph.hpp>

using namespace esc;

TEST(TXT_01_single_scalar_encoding_allocates_nothing)
{
    Utf8CodePoint encoded{};
    std::size_t allocations=0;
    {
        contract::AllocationScope scope;
        encoded=encode_utf8(U'👽');
        allocations=scope.count();
    }
    CHECK(allocations==0 && encoded.view()=="\xf0\x9f\x91\xbd");
}
TEST(GLY_01_named_in_place_styling_allocates_nothing)
{
    GlyphString values=glyphs("hello界");
    std::size_t allocations=0;
    {
        contract::AllocationScope scope;
        style_in_place(values,Trait::Bold);
        allocations=scope.count();
    }
    CHECK(allocations==0);
    CHECK(values==glyphs("hello界",Style{.traits=Trait::Bold}));
}

TEST(TXT_02_allocation_failure_leaves_append_conversion_unchanged)
{
    std::string input(1024,'a');
    std::u32string destination=U"prefix";
    auto before=destination;
    {
        contract::AllocationScope scope{true};
        CHECK_THROWS(std::bad_alloc,append_utf32(destination,input));
    }
    CHECK(destination==before);
}
TEST(GLY_01_allocation_failure_leaves_glyph_append_unchanged)
{
    GlyphString destination{{U'x'}};
    auto before=destination;
    std::string input(1024,'a');
    {
        contract::AllocationScope scope{true};
        CHECK_THROWS(std::bad_alloc,append_glyphs(destination,input));
    }
    CHECK(destination==before);
}

TEST(TXT_02_reserved_conversion_appends_allocate_nothing)
{
    std::string utf8="prefix";
    std::u32string utf32=U"prefix";
    utf8.reserve(1024); utf32.reserve(1024);
    std::size_t allocations=0;
    {
        contract::AllocationScope scope;
        append_utf8(utf8,U"a界👽");
        append_utf32(utf32,"a界👽");
        allocations=scope.count();
    }
    CHECK(allocations==0);
    CHECK(utf8=="prefixa界👽" && utf32==U"prefixa界👽");
}
TEST(GLY_01_reserved_text_and_overlapping_span_appends_allocate_nothing)
{
    GlyphString values=glyphs("ab");
    values.reserve(1024);
    std::span<Glyph const> original{values.data(),2};
    std::size_t allocations=0;
    {
        contract::AllocationScope scope;
        values+=original;
        append_glyphs(values,"界👽");
        append_glyphs(values,U"c");
        allocations=scope.count();
    }
    CHECK(allocations==0);
    CHECK(values==glyphs("abab界👽c"));
}
TEST(GLY_01_iterator_append_has_no_hidden_allocation)
{
    std::array<Glyph,3> output;
    std::size_t allocations=0;
    {
        contract::AllocationScope scope;
        append_glyphs_to(output.begin(),"a界👽");
        allocations=scope.count();
    }
    CHECK(allocations==0);
    CHECK(GlyphString(output.begin(),output.end())==glyphs("a界👽"));
}
TEST(GLY_01_owned_concatenation_allocates_only_its_result)
{
    auto left=glyphs("abc");
    auto right=glyphs("界👽");
    GlyphString result;
    std::size_t allocations=0;
    {
        contract::AllocationScope scope;
        result=left+right;
        allocations=scope.count();
    }
    CHECK(allocations==1 && result==glyphs("abc界👽"));
}

TEST(TXT_02_utf8_append_allocation_failure_preserves_contents)
{
    std::u32string input(1024,U'界');
    std::string output="prefix";
    auto const before=output;
    {
        contract::AllocationScope scope{true};
        CHECK_THROWS(std::bad_alloc,append_utf8(output,input));
    }
    CHECK(output==before);
}
TEST(GLY_01_utf32_and_self_append_allocation_failures_preserve_contents)
{
    GlyphString output=glyphs("prefix");
    output.shrink_to_fit();
    auto const before=output;
    std::u32string input(1024,U'界');
    {
        contract::AllocationScope scope{true};
        CHECK_THROWS(std::bad_alloc,append_glyphs(output,input));
        CHECK_THROWS(std::bad_alloc,(output+=output));
    }
    CHECK(output==before);
}
