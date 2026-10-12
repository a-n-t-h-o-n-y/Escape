#pragma once

#include <chrono>
#include <cstddef>
#include <esc/event.hpp>
#include <optional>
#include <span>
#include <string>

namespace esc {
/// Opt-in mouse, focus, paste, reply, and keyboard grammars.
struct InputProtocols {
    MouseProtocol mouse = MouseProtocol::None;
    bool focus = false;
    bool bracketed_paste = false;
    bool replies = false;
    KeyboardProtocol keyboard = KeyboardProtocol::Legacy;
};

/// Escape/sequence deadlines and bounds on incomplete input and paste chunks.
struct ParserOptions {
    std::chrono::milliseconds escape_timeout{25}, sequence_timeout{250};
    std::size_t sequence_limit = 256, paste_chunk_size = 4096;
};

/// Consumed bytes and at most one event. Advance input even without an event.
struct ParseResult {
    std::size_t consumed;
    std::optional<Event> event;
};

enum class PendingTimeout { Escape, Sequence };

/// Owns a bounded incomplete unit, never an event queue. No clock or I/O.
class Parser {
   public:
    /// Create empty parsing state with the supplied limits and protocols.
    explicit Parser(ParserOptions options = {}, InputProtocols protocols = {});

    /// Consume a prefix and return at most one event. Advance by consumed.
    auto next(std::span<char const> input) -> ParseResult;

    /// Return the kind of deadline needed for the current incomplete unit.
    auto pending_timeout() const noexcept -> std::optional<PendingTimeout>;

    /// Resolve the incomplete unit after its reported deadline has elapsed.
    auto expire() -> std::optional<Event>;

    /// Signal EOF and drain incomplete input; call until no event remains.
    auto finish() -> std::optional<Event>;

    /// Use these protocols for new units; keep the current unit under its old grammar.
    void configure(InputProtocols protocols);

    /// Clear buffered input and EOF state while retaining protocol configuration.
    void reset() noexcept;

   private:
    friend class Reader;
    // Private transport grammar: never enabled for arbitrary terminal bytes.
    bool native_function_keys_ = false;

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
