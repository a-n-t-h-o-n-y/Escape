#pragma once
#include <chrono>
#include <cstddef>
#include <esc/event.hpp>
#include <optional>
#include <span>
#include <string>

namespace esc {
struct InputProtocols {
    MouseProtocol mouse = MouseProtocol::None;
    bool focus = false;
    bool bracketed_paste = false;
};
struct ParserOptions {
    std::chrono::milliseconds escape_timeout{25}, sequence_timeout{250};
    std::size_t sequence_limit = 256, paste_chunk_size = 4096;
};
enum class ParseStatus { Event, NeedInput };
struct ParseResult {
    std::size_t consumed;
    ParseStatus status;
    std::optional<Event> event;
};
enum class PendingTimeout { Escape, Sequence };

// Owns only a bounded incomplete unit, never an event queue. No clock or I/O.
class Parser {
   public:
    explicit Parser(ParserOptions options = {}, InputProtocols protocols = {});
    auto next(std::span<char const> input) -> ParseResult;
    auto pending_timeout() const noexcept -> std::optional<PendingTimeout>;
    auto expire() -> std::optional<Event>;
    auto finish() -> std::optional<Event>;
    void configure(InputProtocols protocols);
    void reset() noexcept;

   private:
    enum class State {
        Ground,
        Escape,
        Utf8,
        Csi,
        Ss3,
        LegacyMouse,
        String,
        Paste,
        PasteEnding
    };
    ParserOptions options_;
    InputProtocols protocols_, active_;
    State state_ = State::Ground;
    std::string buffer_;
    unsigned utf8_width_ = 0;
    bool alt_ = false, overflow_ = false, string_osc_ = false, string_escape_ = false;
    bool paste_first_ = true, paste_aborted_ = false, eof_ = false;
    auto unknown(InputProblem problem, bool final = true) -> Event;
    auto paste_chunk(bool last) -> Event;
    auto complete_sequence() -> Event;
};
}  // namespace esc
