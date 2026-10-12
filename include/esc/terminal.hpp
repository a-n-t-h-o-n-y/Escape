#pragma once
#include <array>
#include <chrono>
#include <esc/capabilities.hpp>
#include <esc/io.hpp>
#include <memory>
#include <optional>
#include <system_error>

namespace esc {
enum class Echo { On, Off };
enum class InputBuffer { Canonical, Immediate };
enum class Signals { On, Off };
struct Streams {
    std::FILE* input = stdin;
    std::FILE* output = stdout;
};
struct ProtocolState {
    ScreenBuffer screen_buffer = ScreenBuffer::Normal;
    CursorMode cursor = CursorMode::Show;
    MouseReporting mouse = {MouseMode::Off, MouseProtocol::None};
    FocusReporting focus = FocusReporting::Off;
    BracketedPaste paste = BracketedPaste::Off;
};
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
    static auto interactive() -> TerminalOptions;
};
class SessionError : public std::system_error {
   public:
    SessionError(std::error_code primary, std::error_code rollback)
        : std::system_error{primary, "Terminal: transition failed"}, rollback_{rollback}
    {}
    auto rollback_error() const noexcept -> std::error_code { return rollback_; }

   private:
    std::error_code rollback_;
};
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

class Terminal {
   public:
    static auto open(TerminalOptions options = {}) -> Terminal;
    static auto open(Streams streams, TerminalOptions options = {}) -> Terminal;
    Terminal(Terminal&&) noexcept;
    auto operator=(Terminal&&) -> Terminal& = delete;
    Terminal(Terminal const&) = delete;
    auto operator=(Terminal const&) -> Terminal& = delete;
    ~Terminal() noexcept;
    auto profile() const -> TerminalProfile const&;
    auto profile_report() const -> ProfileReport const&;
    auto output() -> Output&;
    template <Writable... Values>
    void write(Values const&... values)
    { output().write(values...); }
    template <DrawingCommand... Commands>
    void send(Commands const&... commands)
    {
        auto const prepared = detail::prepare_pack(profile(), commands...);
        auto& stream = output();
        std::apply(
            [&](auto const&... command) {
                auto emit = [&](auto const& value) {
                    std::array<char, 256> bytes;
                    detail::SequenceWriter measure;
                    detail::encode(measure, value);
                    if (measure.size() > bytes.size())
                        throw std::length_error{
                            "Terminal: command exceeds encoder bound"};
                    detail::SequenceWriter writer{bytes.data()};
                    detail::encode(writer, value);
                    stream.write(std::string_view{bytes.data(), writer.size()});
                };
                (emit(command), ...);
            },
            prepared);
    }
    void flush();
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
    auto area() const -> Area;
    auto read() -> ReadResult;
    auto read(std::chrono::milliseconds timeout) -> ReadResult;
    void close();
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
