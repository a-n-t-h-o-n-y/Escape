#pragma once
#include <chrono>
#include <esc/sequence.hpp>
#include <optional>
#include <variant>
#include <vector>

namespace esc {
struct DeviceAttributesQuery {};
struct KeyboardQuery {};
struct ModeQuery {
    unsigned mode;
};
using Query = std::variant<DeviceAttributesQuery, KeyboardQuery, ModeQuery>;
struct DeviceAttributesReply {
    std::vector<unsigned> attributes;
    friend auto operator==(DeviceAttributesReply const&, DeviceAttributesReply const&)
        -> bool = default;
};
struct KeyboardReply {
    unsigned flags;
    friend auto operator==(KeyboardReply const&, KeyboardReply const&)
        -> bool = default;
};
enum class ModeStatus { Unrecognized, Set, Reset, PermanentlySet, PermanentlyReset };
struct ModeReply {
    unsigned mode;
    ModeStatus status;
    friend auto operator==(ModeReply const&, ModeReply const&) -> bool = default;
};
using Reply = std::variant<DeviceAttributesReply, KeyboardReply, ModeReply>;
struct TerminalReply {
    Reply value;
    friend auto operator==(TerminalReply const&, TerminalReply const&)
        -> bool = default;
};
enum class QueryStatus { Reply, Timeout, EndOfInput, InputPending, Rejected };
struct QueryResult {
    QueryStatus status;
    std::optional<Reply> reply;
};
// No hidden I/O. Validate before modifying caller storage.
auto query_to(std::span<char> output, Query const& query) -> EncodeResult;
auto escape_query(Query const& query) -> std::string;
}  // namespace esc
