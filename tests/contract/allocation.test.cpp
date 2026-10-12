#include "support/allocations.hpp"
#include "support/fixtures.hpp"

using namespace esc;

TEST(PAR_01_normal_key_and_mouse_parsing_allocate_nothing_after_setup)
{
    Parser parser{{}, {.mouse = MouseProtocol::Sgr}};
    std::string_view bytes = "\x1b[<0;1;1M";
    ParseResult key{};
    ParseResult mouse{};
    std::size_t allocations = 0;
    {
        contract::AllocationScope scope;
        key = parser.next(std::span<char const>{"a", 1});
        mouse = parser.next(std::span<char const>{bytes.data(), bytes.size()});
        allocations = scope.count();
    }
    CHECK(allocations == 0);
    CHECK(key.event == std::optional<Event>{KeyEvent{U'a'}});
    CHECK(mouse.event == std::optional<Event>{MousePress{{0, 0}, MouseButton::Left}});
}

TEST(PAR_01_fragmented_unicode_named_keys_and_focus_allocate_nothing)
{
    Parser parser{{}, {.focus = true}};
    std::size_t allocations = 0, events = 0;
    std::string_view bytes = "界\x1b[1;16A\x1bOP\x1b[I";
    {
        contract::AllocationScope scope;
        for (auto byte : bytes) {
            auto result = parser.next(std::span<char const>{&byte, 1});
            events += result.event.has_value();
            CHECK(result.consumed == 1);
        }
        allocations = scope.count();
    }
    CHECK(allocations == 0 && events == 4);
}
