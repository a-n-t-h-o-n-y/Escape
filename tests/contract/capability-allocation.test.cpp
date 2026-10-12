#include "support/capabilities.hpp"
#include "support/allocations.hpp"

using namespace esc;

TEST(BUF_02_bounded_encoding_and_capacity_failure_allocate_nothing)
{
    std::array<char,256> bytes;
    auto profile=contract::full_profile();
    Style style{.background=RGB{0x010203},.foreground=XColor::BrightRed,.traits=contract::full_traits()};
    std::size_t allocations=0;
    EncodeResult result{};
    EncodeResult short_result{};
    {
        contract::AllocationScope scope;
        result=encode_to(bytes,profile,Cursor{4,2},style);
        short_result=encode_to(std::span<char>{},profile,Cursor{4,2},style);
        allocations=scope.count();
    }
    CHECK(allocations==0 && result.written==result.required);
    CHECK(short_result.written==0 && short_result.required==result.required);
}
TEST(BUF_01_append_with_capacity_allocates_nothing)
{
    std::string frame="prefix";
    frame.reserve(4096);
    auto profile=contract::full_profile();
    std::size_t allocations=0;
    {
        contract::AllocationScope scope;
        append_encoded(frame,profile,Cursor{4,2},Style{.foreground=RGB{0x010203}},ResetStyle{});
        allocations=scope.count();
    }
    CHECK(allocations==0);
    CHECK(frame.starts_with("prefix\x1b[3;5H"));
}
TEST(BUF_01_allocation_failure_preserves_existing_string_contents)
{
    std::string frame(64,'p');
    auto const before=frame;
    auto profile=contract::full_profile();
    // More commands than existing capacity guarantee a required allocation.
    while (frame.size()<frame.capacity()) frame.push_back('p');
    auto const filled=frame;
    {
        contract::AllocationScope scope{true};
        CHECK_THROWS(std::bad_alloc,append_encoded(frame,profile,Cursor{},contract::full_traits(),Style{}));
    }
    CHECK(frame==filled && frame.starts_with(before));
}

TEST(BUF_02_quantized_profile_encoding_allocates_nothing)
{
    auto profile=contract::full_profile();
    profile.colors=ColorSupport::Indexed256;
    profile.traits=contract::basic_traits();
    std::array<char,256> bytes;
    std::string frame="prefix";
    frame.reserve(512);
    Style style{.foreground=RGB{0x123456},.traits=Trait::DoubleUnderline};
    std::size_t allocations=0;
    {
        contract::AllocationScope scope;
        auto result=encode_to(bytes,profile,style);
        CHECK(result.written==result.required);
        append_encoded(frame,profile,style);
        allocations=scope.count();
    }
    CHECK(allocations==0);
    CHECK(frame=="prefix"+encode(profile,style));
}
