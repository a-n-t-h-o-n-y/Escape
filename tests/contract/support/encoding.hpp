#pragma once

#include "test.hpp"
#include <esc/sequence.hpp>
#include <esc/area.hpp>
#include <esc/terminal.hpp>
#include <algorithm>
#include <array>
#include <concepts>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace contract {
inline auto full_traits() -> esc::Traits
{
    return esc::Trait::Bold | esc::Trait::Dim | esc::Trait::Italic |
           esc::Trait::Underline | esc::Trait::Blink | esc::Trait::Inverse |
           esc::Trait::Invisible | esc::Trait::CrossedOut | esc::Trait::DoubleUnderline;
}
inline auto basic_traits() -> esc::Traits
{
    return esc::Trait::Bold | esc::Trait::Dim | esc::Trait::Underline |
           esc::Trait::Blink | esc::Trait::Inverse | esc::Trait::Invisible;
}
// The oracle is supplied by a literal/table, never obtained from escape().
// Run the same expectation through every encoder destination and capacity case.
template<class... Commands>
void encodes(std::string_view expected, Commands const&... commands)
{
    CHECK(esc::escape(commands...) == expected);
    std::string appended{"existing\0bytes", 14};
    auto const prefix = appended;
    esc::append_escape(appended, commands...);
    CHECK(appended == prefix + std::string{expected});

    std::vector<char> buffer(expected.size()+3, '#');
    auto const encoded = esc::escape_to(std::span{buffer}, commands...);
    CHECK(encoded.written == expected.size());
    CHECK(encoded.required == expected.size());
    CHECK(std::string_view{buffer.data(), encoded.written} == expected);
    CHECK(std::all_of(buffer.begin()+encoded.written, buffer.end(), [](char c) { return c=='#'; }));

    std::vector<char> exact(expected.size(), '#');
    auto const exact_result = esc::escape_to(std::span{exact}, commands...);
    CHECK(exact_result.written == expected.size());
    CHECK(exact_result.required == expected.size());
    CHECK(std::equal(exact.begin(), exact.end(), expected.begin(), expected.end()));

    if (!expected.empty()) {
        std::vector<char> short_buffer(expected.size()-1, '#');
        auto const before = short_buffer;
        auto const short_result = esc::escape_to(std::span{short_buffer}, commands...);
        CHECK(short_result.written == 0);
        CHECK(short_result.required == expected.size());
        CHECK(short_buffer == before);
    }
}

} // namespace contract
