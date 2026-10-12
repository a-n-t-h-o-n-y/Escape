#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <concepts>
#include <cstdint>
#include <cstdio>
#include <esc/detail/unicode.hpp>
#include <esc/parser.hpp>
#include <esc/sequence.hpp>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

namespace esc {
template <class T>
concept Writable = Encodable<T>;

/// Raw byte output on a borrowed FILE stream; no capability adaptation.
class Output {
   public:
    /// Borrow an output stream without closing or changing its mode.
    explicit Output(std::FILE* stream = stdout);

    /// Validate the whole pack, then write it. Adds no separators or flush.
    template <Writable... Values>
    void write(Values const&... values)
    { write_values(nullptr, values...); }

    /// Flush the borrowed stdio stream; errors throw.
    void flush();

   private:
    friend class Terminal;
    std::FILE* stream_;

    void write_one(std::string_view bytes);

    struct Batch {
        Output& output;
        std::array<char, 1024> bytes;
        std::size_t used = 0;

        void drain()
        {
            output.write_one({bytes.data(), used});
            used = 0;
        }

        static void accept(void* context, std::string_view input)
        {
            auto& batch = *static_cast<Batch*>(context);
            while (!input.empty()) {
                if (!batch.used && input.size() >= batch.bytes.size()) {
                    batch.output.write_one(input);
                    return;
                }
                auto count = std::min(input.size(), batch.bytes.size() - batch.used);
                std::copy_n(input.data(), count, batch.bytes.data() + batch.used);
                batch.used += count;
                input.remove_prefix(count);
                if (batch.used == batch.bytes.size()) batch.drain();
            }
        }
    };

    template <Writable... Values>
    void write_values(TerminalProfile const* profile, Values const&... values)
    {
        if (profile) detail::validate_profile(*profile);
        detail::SequenceWriter measure;
        detail::encode_values(measure, profile, values...);
        Batch batch{*this, {}, 0};
        detail::SequenceWriter writer{&batch, Batch::accept};
        detail::encode_values(writer, profile, values...);
        batch.drain();
    }
};

/// Write raw values to stdout without adding a newline or flushing.
template <Writable... Values>
void write(Values const&... values)
{ Output{}.write(values...); }

/// Flush stdout.
void flush();

/// One input outcome: an event, timeout, or exhausted input.
using ReadResult = std::variant<Event, Timeout, EndOfInput>;

enum class WaitSourceKind { None, FileDescriptor, WindowsHandle };

/// A borrowed descriptor or Windows handle. Do not read or close it directly.
struct WaitSource {
    WaitSourceKind kind = WaitSourceKind::None;
    std::intptr_t value = 0;  // borrowed; never close it or read it directly
};

/// Current readiness, borrowed wait source, and optional steady-clock deadline.
struct WaitState {
    WaitSource source;
    bool ready = false;  // event/EOF is held, or input can progress without waiting
    std::optional<std::chrono::steady_clock::time_point> deadline;
};

/// Incremental input with bounded parsing state and steady-clock deadlines.
class Reader {
   public:
    /// Borrow input and configure parsing. Do not mix with another stream reader.
    explicit Reader(std::FILE* input = stdin,
                    ParserOptions options = {},
                    InputProtocols protocols = {});

    ~Reader();

    Reader(Reader&&) noexcept;

    auto operator=(Reader&&) noexcept -> Reader&;

    Reader(Reader const&) = delete;

    auto operator=(Reader const&) -> Reader& = delete;

    /// Wait for the next event or EOF, resolving parser deadlines as needed.
    auto read() -> ReadResult;

    /// Read with a deadline; zero polls and negative durations throw.
    auto read(std::chrono::milliseconds timeout) -> ReadResult;

    /// Change protocols for new units, preserving one already in progress.
    void configure(InputProtocols protocols);

    /// Get readiness and deadlines for integration with a host event loop.
    auto wait_state() -> WaitState;

   private:
    friend class Terminal;

    void prepare_queries();

    void stop_queries() noexcept;

    void configure_session(InputProtocols protocols,
                           MouseMode mode,
                           int output_descriptor);

    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}  // namespace esc
