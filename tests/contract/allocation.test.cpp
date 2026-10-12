#include "support/fixtures.hpp"
#include "support/allocations.hpp"

using namespace esc;

TEST(PAR_01_normal_key_and_mouse_parsing_allocate_nothing_after_setup)
{
    Parser parser{{},{.mouse=MouseProtocol::Sgr}};
    std::string_view bytes="\x1b[<0;1;1M";
    ParseResult key{};
    ParseResult mouse{};
    std::size_t allocations=0;
    {
        contract::AllocationScope scope;
        key=parser.next(std::span<char const>{"a",1});
        mouse=parser.next(std::span<char const>{bytes.data(),bytes.size()});
        allocations=scope.count();
    }
    CHECK(allocations==0);
    CHECK(key.event==std::optional<Event>{KeyPress{U'a'}});
    CHECK(mouse.event==std::optional<Event>{MousePress{{0,0},MouseButton::Left}});
}
