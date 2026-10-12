#pragma once

#include <array>
#include <chrono>
#include <esc/capabilities.hpp>
#include <esc/io.hpp>
#include <memory>
#include <optional>
#include <system_error>

namespace esc {

/// Whether terminal input is echoed by the OS.
enum class Echo { On, Off };

/// OS line editing, or immediate byte/key delivery.
enum class InputBuffer { Canonical, Immediate };

/// Whether terminal input characters generate OS signals.
enum class Signals { On, Off };

/// Borrowed input/output streams on the same terminal device.
struct Streams {
    std::FILE* input = stdin;
    std::FILE* output = stdout;
};

/// Inherited protocol modes to restore when the session closes.
struct ProtocolState {
    ScreenBuffer screen_buffer = ScreenBuffer::Normal;
    CursorMode cursor = CursorMode::Show;
    MouseReporting mouse = {MouseMode::Off, MouseProtocol::None};
    FocusReporting focus = FocusReporting::Off;
    BracketedPaste paste = BracketedPaste::Off;
    SynchronizedOutput synchronized_output = SynchronizedOutput::Off;
};

/// Requested mode changes. Unset options preserve the captured state.
struct TerminalOptions {
    std::optional<Echo> echo;
    std::optional<InputBuffer> input_buffer;
    std::optional<Signals> signals;
    std::optional<ScreenBuffer> screen_buffer;
    std::optional<CursorMode> cursor;
    std::optional<MouseMode> mouse;
    std::optional<FocusReporting> focus;
    std::optional<BracketedPaste> paste;
    std::optional<TerminalProfile> profile;
    ProtocolState restore_modes = {};
    ParserOptions parser = {};
    std::optional<std::chrono::milliseconds> resize_interval =
        std::chrono::milliseconds{100};

    /// Request immediate input, echo off, alternate screen, and a hidden cursor.
    static auto interactive() -> TerminalOptions;
};

/// A failed transition, with an optional secondary rollback error.
class SessionError : public std::system_error {
   public:
    SessionError(std::error_code primary, std::error_code rollback)
        : std::system_error{primary, "Terminal: transition failed"}, rollback_{rollback}
    {}

    /// Return the error from restoring a failed transition, if any.
    auto rollback_error() const noexcept -> std::error_code { return rollback_; }

   private:
    std::error_code rollback_;
};

/// A mode that Terminal::set can own and restore.
template <class T>
concept Setting = detail::AnyOf<T,
                                Echo,
                                InputBuffer,
                                Signals,
                                ScreenBuffer,
                                CursorMode,
                                MouseMode,
                                FocusReporting,
                                BracketedPaste>;

/// One query and its outcome, kept separate from capability assumptions.
struct ProbeObservation {
    Query request;
    QueryResult result;
    EvidenceSource source = EvidenceSource::Probe;
};

/// Collected keyboard and private-mode observations.
struct ProbeReport {
    std::vector<ProbeObservation> observations;
};

/// Scoped output frame. Nested frames share the outermost flush/synchronization.
class Frame {
   public:
    Frame(Frame&&) noexcept;

    auto operator=(Frame&&) -> Frame& = delete;

    Frame(Frame const&) = delete;

    auto operator=(Frame const&) -> Frame& = delete;

    /// Attempt to end the frame without throwing.
    ~Frame() noexcept;

    /// End the frame and report flush or synchronization errors.
    void close();

    /// Whether this active frame uses synchronized output.
    auto synchronized() const noexcept -> bool;

   private:
    friend class Terminal;
    struct State;
    std::weak_ptr<State> state_;
    bool active_ = true, synchronized_ = false;

    Frame(std::shared_ptr<State> const& state, bool synchronized) noexcept;
};

/// A move-only session owning terminal mode changes, not the FILE streams.
/// Close restores captured OS state and the supplied initial protocol modes.
class Terminal {
   public:
    /// Open a stdin/stdout session. Default options leave modes unchanged.
    static auto open(TerminalOptions options = {}) -> Terminal;

    /// Open a session on borrowed streams referring to the same terminal device.
    static auto open(Streams streams, TerminalOptions options = {}) -> Terminal;

    Terminal(Terminal&&) noexcept;

    auto operator=(Terminal&&) -> Terminal& = delete;

    Terminal(Terminal const&) = delete;

    auto operator=(Terminal const&) -> Terminal& = delete;

    /// Attempt to restore session-owned state without throwing.
    ~Terminal() noexcept;

    /// Read the selected capability profile.
    auto profile() const -> TerminalProfile const&;

    /// Read the profile and per-feature evidence/explanations.
    auto profile_report() const -> ProfileReport const&;

    /// Access raw output; values written here bypass capability adaptation.
    auto output() -> Output&;

    /// Draw through the session profile. Raw mode commands are excluded.
    template <Drawable... Values>
    void write(Values const&... values)
    { output().write_values(&profile(), values...); }

    /// Flush session output.
    void flush();

    /// Begin a frame; outermost close flushes and ends synchronization.
    /// Strict policy rejects unavailable synchronization.
    auto frame() -> Frame;

    /// Query mode 2026 and enable frame synchronization only after confirmation.
    auto confirm_synchronized_output(std::chrono::milliseconds timeout =
                                         std::chrono::milliseconds{250}) -> QueryResult;

    /// Expose readiness, a borrowed host wait source, and the next deadline.
    auto wait_state() -> WaitState;

    /// Send and flush a query; its reply arrives through read().
    void request(Query const& query);

    /// Validate/send a query batch and flush once.
    void request(std::span<Query const> queries);

    /// Wait for one matching reply, preserving unrelated input for read().
    auto query(Query const& query, std::chrono::milliseconds timeout) -> QueryResult;

    /// Collect keyboard and mode replies under one shared deadline.
    /// Does not change the profile or enable protocols.
    auto probe(std::chrono::milliseconds timeout = std::chrono::milliseconds{250})
        -> ProbeReport;

    /// Negotiate and confirm Kitty flags; failed confirmation restores old flags.
    auto enable_keyboard(KittyFlags flags = KittyFlags::Disambiguate,
                         std::chrono::milliseconds timeout = std::chrono::milliseconds{
                             250}) -> QueryResult;

    /// Restore captured keyboard flags. Safe to call repeatedly.
    void disable_keyboard();

    /// Read the active keyboard grammar, including inherited Kitty state.
    auto keyboard_protocol() const -> KeyboardProtocol;

    /// Change session-owned settings. Duplicate setting kinds are rejected.
    template <Setting... Settings>
    void set(Settings const&... settings)
    {
        require_open();
        TerminalOptions patch;
        unsigned seen = 0;
        auto assign = [&](auto value) {
            unsigned bit = 0;
            using T = decltype(value);
            if constexpr (std::same_as<T, Echo>) {
                bit = 1;
                patch.echo = value;
            }
            else if constexpr (std::same_as<T, InputBuffer>) {
                bit = 2;
                patch.input_buffer = value;
            }
            else if constexpr (std::same_as<T, Signals>) {
                bit = 4;
                patch.signals = value;
            }
            else if constexpr (std::same_as<T, ScreenBuffer>) {
                bit = 8;
                patch.screen_buffer = value;
            }
            else if constexpr (std::same_as<T, CursorMode>) {
                bit = 16;
                patch.cursor = value;
            }
            else if constexpr (std::same_as<T, MouseMode>) {
                bit = 32;
                patch.mouse = value;
            }
            else if constexpr (std::same_as<T, FocusReporting>) {
                bit = 64;
                patch.focus = value;
            }
            else {
                bit = 128;
                patch.paste = value;
            }
            if (seen & bit) throw std::invalid_argument{"Terminal: duplicate setting"};
            seen |= bit;
        };
        (assign(settings), ...);
        if constexpr (sizeof...(Settings) > 0) apply(patch);
    }

    /// Read terminal dimensions in cells.
    auto area() const -> Area;

    /// Wait for an event or input exhaustion; drain preserved query input first.
    auto read() -> ReadResult;

    /// Read until the deadline. Zero polls; negative durations throw.
    auto read(std::chrono::milliseconds timeout) -> ReadResult;

    /// Restore owned state and report cleanup errors. Repeated close is a no-op.
    void close();

    /// Whether this session still owns its terminal state.
    auto is_open() const noexcept -> bool;

   private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    explicit Terminal(std::unique_ptr<Impl> implementation) noexcept;

    void require_open() const;

    void apply(TerminalOptions const& patch);

    auto read_until(std::optional<std::chrono::milliseconds> timeout) -> ReadResult;
};
}  // namespace esc
