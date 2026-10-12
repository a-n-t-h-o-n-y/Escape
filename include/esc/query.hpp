#pragma once

#include <chrono>
#include <esc/result.hpp>
#include <esc/sequence.hpp>
#include <optional>
#include <variant>
#include <vector>

namespace esc {
/// Request primary device attributes (DA1).
struct DeviceAttributesQuery {};

/// Request the active Kitty keyboard flags.
struct KeyboardQuery {};

/// Request DECRQM status for one positive private-mode number.
struct ModeQuery {
    unsigned mode;
};

using Query = std::variant<DeviceAttributesQuery, KeyboardQuery, ModeQuery>;

/// DA1 attributes reported by the terminal; these are not a capability guarantee.
struct DeviceAttributesReply {
    std::vector<unsigned> attributes;
    friend auto operator==(DeviceAttributesReply const&, DeviceAttributesReply const&)
        -> bool = default;
};

/// Kitty flags reported by the terminal.
struct KeyboardReply {
    unsigned flags;
    friend auto operator==(KeyboardReply const&, KeyboardReply const&)
        -> bool = default;
};

enum class ModeStatus { Unrecognized, Set, Reset, PermanentlySet, PermanentlyReset };

/// Reported status of the requested private mode.
struct ModeReply {
    unsigned mode;
    ModeStatus status;
    friend auto operator==(ModeReply const&, ModeReply const&) -> bool = default;
};

using Reply = std::variant<DeviceAttributesReply, KeyboardReply, ModeReply>;

/// An unsolicited terminal reply delivered through the event stream.
struct TerminalReply {
    Reply value;
    friend auto operator==(TerminalReply const&, TerminalReply const&)
        -> bool = default;
};

/// A negative acknowledgment keeps the actual response for diagnostics.
struct Rejected {
    Reply reply;
    friend auto operator==(Rejected const&, Rejected const&) -> bool = default;
};

/// A typed reply, deadline/EOF/pending result, or a retained rejection.
using QueryResult = std::variant<Reply, Timeout, EndOfInput, InputPending, Rejected>;

/// No hidden I/O. Validate before modifying caller storage.
auto query_to(std::span<char> output, Query const& query) -> EncodeResult;

/// Validate a query and return its request bytes, without terminal I/O.
auto encode_query(Query const& query) -> std::string;
}  // namespace esc
