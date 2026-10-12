#pragma once
#include <esc/io.hpp>
namespace contract {
// Test conveniences only. Production callers visit the public variants directly.
enum class Status {
    Event,
    NeedInput,
    Reply,
    Timeout,
    EndOfInput,
    InputPending,
    Rejected
};
inline auto status(esc::ReadResult const& value) -> Status
{
    if (std::holds_alternative<esc::Event>(value)) return Status::Event;
    if (std::holds_alternative<esc::Timeout>(value)) return Status::Timeout;
    return Status::EndOfInput;
}
inline auto status(esc::QueryResult const& value) -> Status
{
    if (std::holds_alternative<esc::Reply>(value)) return Status::Reply;
    if (std::holds_alternative<esc::Timeout>(value)) return Status::Timeout;
    if (std::holds_alternative<esc::EndOfInput>(value)) return Status::EndOfInput;
    if (std::holds_alternative<esc::InputPending>(value)) return Status::InputPending;
    return Status::Rejected;
}
inline auto status(esc::ParseResult const& value) -> Status
{ return value.event ? Status::Event : Status::NeedInput; }
inline auto event(esc::ReadResult const& value) -> std::optional<esc::Event>
{
    if (auto event = std::get_if<esc::Event>(&value)) return *event;
    return {};
}
inline auto event(esc::ParseResult& value) -> std::optional<esc::Event>&
{ return value.event; }
inline auto event(esc::ParseResult const& value) -> std::optional<esc::Event> const&
{ return value.event; }
inline auto event(esc::ParseResult&& value) -> std::optional<esc::Event>
{ return std::move(value.event); }
inline auto reply(esc::QueryResult const& value) -> std::optional<esc::Reply>
{
    if (auto reply = std::get_if<esc::Reply>(&value)) return *reply;
    return {};
}
}  // namespace contract
