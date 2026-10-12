#pragma once

#include "capabilities.hpp"

#include <esc/capabilities.hpp>
#include <esc/esc.hpp>
#include <esc/parser.hpp>
#include <esc/text.hpp>

#include <algorithm>
#include <array>
#include <concepts>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace contract {
inline void feed(esc::Parser& parser,
                 std::string_view bytes,
                 std::vector<esc::Event>& events)
{
    auto remaining = std::span<char const>{bytes.data(), bytes.size()};
    std::size_t iterations = 0;
    while (true) {
        CHECK(++iterations <=
              bytes.size() * 4 + 8192);  // Fail rather than hang on no progress.
        auto result = parser.next(remaining);
        CHECK(result.consumed <= remaining.size());
        remaining = remaining.subspan(result.consumed);
        if (result.event.has_value()) {
            CHECK(result.event.has_value());
            events.push_back(std::move(*result.event));
        }
        else {
            CHECK(!result.event.has_value());
            CHECK(remaining.empty());
            return;
        }
    }
}

inline auto parse(std::string_view bytes,
                  esc::InputProtocols protocols = {},
                  esc::ParserOptions options = {}) -> std::vector<esc::Event>
{
    esc::Parser parser{options, protocols};
    std::vector<esc::Event> events;
    feed(parser, bytes, events);
    return events;
}

inline void parses_at_every_split(std::string_view bytes,
                                  std::vector<esc::Event> const& expected,
                                  esc::InputProtocols protocols = {},
                                  esc::ParserOptions options = {})
{
    CHECK(parse(bytes, protocols, options) == expected);
    for (std::size_t split = 0; split <= bytes.size(); ++split) {
        esc::Parser parser{options, protocols};
        std::vector<esc::Event> actual;
        feed(parser, bytes.substr(0, split), actual);
        feed(parser, bytes.substr(split), actual);
        CHECK(actual == expected);
    }
    esc::Parser parser{options, protocols};
    std::vector<esc::Event> actual;
    for (std::size_t i = 0; i < bytes.size(); ++i)
        feed(parser, bytes.substr(i, 1), actual);
    CHECK(actual == expected);
}

inline auto unknown_bytes(std::vector<esc::Event> const& events) -> std::string
{
    std::string result;
    for (auto const& event : events) {
        CHECK(std::holds_alternative<esc::UnknownInput>(event));
        result += std::get<esc::UnknownInput>(event).bytes;
    }
    return result;
}
}  // namespace contract
