#include "support/encoding.hpp"
#include "support/allocations.hpp"

using namespace esc;

TEST(BUF_02_raw_encoding_and_capacity_failure_allocate_nothing)
{
    std::array<char,256> bytes;
    Style style{.background=RGB{0x010203},.foreground=XColor::BrightRed,.traits=contract::full_traits()};
    std::size_t allocations=0;
    EncodeResult result{};
    EncodeResult short_result{};
    {
        contract::AllocationScope scope;
        result=encode_to(bytes,Cursor{4,2},style);
        short_result=encode_to(std::span<char>{},Cursor{4,2},style);
        allocations=scope.count();
    }
    CHECK(allocations==0 && result.written==result.required);
    CHECK(short_result.written==0 && short_result.required==result.required);
}
TEST(BUF_01_reserved_raw_append_allocates_nothing)
{
    std::string frame="prefix";
    frame.reserve(4096);
    std::size_t allocations=0;
    {
        contract::AllocationScope scope;
        append_encoded(frame,Cursor{4,2},Style{.foreground=RGB{0x010203}},ResetStyle{});
        allocations=scope.count();
    }
    CHECK(allocations==0);
    CHECK(frame.starts_with("prefix\x1b[3;5H"));
}
TEST(BUF_01_raw_append_allocation_failure_preserves_contents)
{
    std::string frame(64,'p');
    while (frame.size()<frame.capacity()) frame.push_back('p');
    auto const before=frame;
    {
        contract::AllocationScope scope{true};
        CHECK_THROWS(std::bad_alloc,append_encoded(frame,Cursor{},contract::full_traits(),Style{}));
    }
    CHECK(frame==before);
}
TEST(BUF_01_raw_append_uses_one_destination_allocation_for_a_pack)
{
    std::string frame;
    auto const traits=contract::full_traits();
    std::size_t allocations=0;
    {
        contract::AllocationScope scope;
        append_encoded(frame,traits,traits,traits,traits,traits,traits,traits,traits);
        allocations=scope.count();
    }
    CHECK(allocations==1);
    CHECK(frame==encode(traits)+encode(traits)+encode(traits)+encode(traits)+
                 encode(traits)+encode(traits)+encode(traits)+encode(traits));
}
TEST(ENC_04_invalid_values_validate_the_whole_pack_before_mutation)
{
    auto invalid=fg(static_cast<TermColor>(true));
    std::string output="unchanged";
    CHECK_THROWS(std::invalid_argument,append_encoded(output,ResetStyle{},invalid));
    CHECK(output=="unchanged");
    std::array<char,512> bytes;
    bytes.fill('#');
    auto before=bytes;
    CHECK_THROWS(std::invalid_argument,encode_to(bytes,MouseReporting{MouseMode::Move,MouseProtocol::Sgr},invalid));
    CHECK(bytes==before);
    CHECK_THROWS(std::invalid_argument,encode_to(std::span<char>{},invalid));
    CHECK_THROWS(std::invalid_argument,encode(static_cast<FocusReporting>(99)));
    CHECK_THROWS(std::invalid_argument,encode(static_cast<BracketedPaste>(99)));
    CHECK_THROWS(std::invalid_argument,encode(MouseReporting{static_cast<MouseMode>(99),MouseProtocol::Sgr}));
    CHECK_THROWS(std::invalid_argument,encode(static_cast<Trait>(1)));
}
TEST(ENC_03_every_trait_subset_has_canonical_order_and_no_duplicates)
{
    constexpr Trait traits[]={Trait::Bold,Trait::Dim,Trait::Italic,Trait::Underline,
        Trait::Blink,Trait::Inverse,Trait::Invisible,Trait::CrossedOut,Trait::DoubleUnderline};
    constexpr std::string_view parameters[]={";1",";2",";3",";4",";5",";7",";8",";9",";21"};
    for (unsigned subset=0;subset<(1u<<9);++subset) {
        Traits value;
        for (unsigned i=0;i<9;++i) if (subset & (1u<<i)) value.insert(traits[i]);
        std::string expected;
        for (unsigned i=0;i<8;++i) {
            if (i==3 && (subset & (1u<<8))) expected+=";21";
            else if (subset & (1u<<i)) expected+=parameters[i];
        }
        if (!expected.empty()) {
            expected.erase(0, 1); // Drop the leading delimiter from the independent table.
            expected = "\x1b[" + expected + 'm';
        }
        contract::encodes(expected,value);
    }
}
