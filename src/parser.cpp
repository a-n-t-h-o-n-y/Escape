#include <algorithm>
#include <array>
#include <charconv>
#include <esc/detail/unicode.hpp>
#include <esc/parser.hpp>
#include <limits>
#include <stdexcept>
#include "detail/protocol.hpp"

namespace esc {
namespace {
void validate_protocols(InputProtocols p)
{
    if (p.keyboard != KeyboardProtocol::Legacy && p.keyboard != KeyboardProtocol::Kitty)
        throw std::invalid_argument{"Parser: invalid keyboard protocol"};
    switch (p.mouse) {
        case MouseProtocol::None:
        case MouseProtocol::Legacy:
        case MouseProtocol::Sgr:
        case MouseProtocol::Urxvt: return;
    }
    throw std::invalid_argument{"Parser: invalid mouse protocol"};
}
auto scalar_key(char32_t scalar, bool alt = false) -> KeyEvent
{
    Key key = scalar;
    switch (scalar) {
        case 9: key = KeyCode::Tab; break;
        case 13: key = KeyCode::Enter; break;
        case 8:
        case 127: key = KeyCode::Backspace; break;
        case 27: key = KeyCode::Escape; break;
    }
    return {key, Modifiers{.alt = alt}};
}
auto utf8_width(unsigned char byte) -> unsigned
{
    if (byte >= 0xc2 && byte <= 0xdf) return 2;
    if (byte >= 0xe0 && byte <= 0xef) return 3;
    if (byte >= 0xf0 && byte <= 0xf4) return 4;
    return 0;
}
struct Parameters {
    std::array<std::uint64_t, 3> values{};
    std::size_t size = 0;
    bool valid = true;
    explicit Parameters(std::string_view text)
    {
        if (text.empty()) return;
        while (true) {
            auto pos = text.find(';');
            auto field = text.substr(0, pos);
            if (size == values.size() || field.empty()) {
                valid = false;
                return;
            }
            auto [end, error] = std::from_chars(
                field.data(), field.data() + field.size(), values[size]);
            if (error != std::errc{} || end != field.data() + field.size()) {
                valid = false;
                return;
            }
            ++size;
            if (pos == text.npos) return;
            text.remove_prefix(pos + 1);
        }
    }
};
auto modifiers(std::uint64_t parameter) -> Modifiers
{
    auto bits = parameter - 1;
    return {bool(bits & 1), bool(bits & 4), bool(bits & 2), bool(bits & 8)};
}
auto named_key(char final, bool keypad) -> std::optional<KeyCode>
{
    switch (final) {
        case 'A': return KeyCode::ArrowUp;
        case 'B': return KeyCode::ArrowDown;
        case 'C': return KeyCode::ArrowRight;
        case 'D': return KeyCode::ArrowLeft;
        case 'H': return KeyCode::Home;
        case 'F': return KeyCode::End;
        case 'P': return KeyCode::F1;
        case 'Q': return KeyCode::F2;
        case 'R': return KeyCode::F3;
        case 'S': return KeyCode::F4;
    }
    if (!keypad) return {};
    switch (final) {
        case 'M': return KeyCode::KeypadEnter;
        case 'p': return KeyCode::Keypad0;
        case 'q': return KeyCode::Keypad1;
        case 'r': return KeyCode::Keypad2;
        case 's': return KeyCode::Keypad3;
        case 't': return KeyCode::Keypad4;
        case 'u': return KeyCode::Keypad5;
        case 'v': return KeyCode::Keypad6;
        case 'w': return KeyCode::Keypad7;
        case 'x': return KeyCode::Keypad8;
        case 'y': return KeyCode::Keypad9;
        case 'n': return KeyCode::KeypadDecimal;
        case 'k': return KeyCode::KeypadPlus;
        case 'm': return KeyCode::KeypadMinus;
        case 'j': return KeyCode::KeypadMultiply;
        case 'o': return KeyCode::KeypadDivide;
    }
    return {};
}
auto tilde_key(std::uint64_t number, bool native_functions) -> std::optional<KeyCode>
{
    if (native_functions) {
        switch (number) {
            case 42: return KeyCode::F21;
            case 43: return KeyCode::F22;
            case 44: return KeyCode::F23;
            case 45: return KeyCode::F24;
        }
    }
    switch (number) {
        case 1:
        case 7: return KeyCode::Home;
        case 2: return KeyCode::Insert;
        case 3: return KeyCode::Delete;
        case 4:
        case 8: return KeyCode::End;
        case 5: return KeyCode::PageUp;
        case 6: return KeyCode::PageDown;
        case 11: return KeyCode::F1;
        case 12: return KeyCode::F2;
        case 13: return KeyCode::F3;
        case 14: return KeyCode::F4;
        case 15: return KeyCode::F5;
        case 17: return KeyCode::F6;
        case 18: return KeyCode::F7;
        case 19: return KeyCode::F8;
        case 20: return KeyCode::F9;
        case 21: return KeyCode::F10;
        case 23: return KeyCode::F11;
        case 24: return KeyCode::F12;
        case 25: return KeyCode::F13;
        case 26: return KeyCode::F14;
        case 28: return KeyCode::F15;
        case 29: return KeyCode::F16;
        case 31: return KeyCode::F17;
        case 32: return KeyCode::F18;
        case 33: return KeyCode::F19;
        case 34: return KeyCode::F20;
    }
    return {};
}
auto mouse_event(std::uint64_t code,
                 std::uint64_t x,
                 std::uint64_t y,
                 bool release,
                 bool sgr) -> std::optional<Event>
{
    constexpr auto max_position = std::uint64_t{std::numeric_limits<int>::max()} + 1;
    if (!x || !y || x > max_position || y > max_position || code > 255) return {};
    Point at{static_cast<int>(x - 1), static_cast<int>(y - 1)};
    Modifiers mods{bool(code & 4), bool(code & 16), bool(code & 8), false};
    unsigned flags = static_cast<unsigned>(code) & ~28u;
    unsigned button = flags & 3u;
    bool motion = flags & 32u, wheel = flags & 64u, extra = flags & 128u;
    if ((wheel && (motion || extra || release)) || (motion && release)) return {};
    if (wheel) {
        int dx = button == 2 ? -1 : button == 3 ? 1 : 0;
        int dy = button == 0 ? -1 : button == 1 ? 1 : 0;
        return Event{MouseWheel{at, dx, dy, mods}};
    }
    std::optional<MouseButton> held;
    if (extra)
        held = static_cast<MouseButton>(static_cast<unsigned>(MouseButton::Button8) +
                                        button);
    else if (button != 3)
        held = static_cast<MouseButton>(button);
    if (motion) return Event{MouseMove{at, held, mods}};
    if (release || (!sgr && button == 3 && !extra))
        return Event{MouseRelease{at, held, mods}};
    if (!held) return {};
    return Event{MousePress{at, *held, mods}};
}
}  // namespace

Parser::Parser(ParserOptions options, InputProtocols protocols)
    : options_{options}, protocols_{protocols}
{
    validate_protocols(protocols);
    if (options.sequence_limit < 16 || options.sequence_limit > 4096 ||
        !options.paste_chunk_size || options.paste_chunk_size > 4096 ||
        options.escape_timeout.count() < 0 || options.sequence_timeout.count() < 0)
        throw std::invalid_argument{"Parser: invalid limits or timeouts"};
    buffer_.reserve(std::max(options.sequence_limit, options.paste_chunk_size + 6));
}
void Parser::configure(InputProtocols p)
{
    validate_protocols(p);
    protocols_ = p;
}
void Parser::reset() noexcept
{
    state_ = State::Ground;
    buffer_.clear();
    eof_ = false;
    overflow_ = false;
    string_escape_ = false;
    alt_ = false;
    paste_first_ = true;
    paste_aborted_ = false;
}
auto Parser::pending_timeout() const noexcept -> std::optional<PendingTimeout>
{
    if (state_ == State::Escape) return PendingTimeout::Escape;
    if (state_ == State::Csi || state_ == State::Ss3 || state_ == State::LegacyMouse ||
        state_ == State::String || (state_ == State::Utf8 && alt_))
        return PendingTimeout::Sequence;
    return {};
}
auto Parser::unknown(InputProblem problem, bool final) -> Event
{
    Event event =
        UnknownInput{buffer_, overflow_ ? InputProblem::TooLong : problem, final};
    buffer_.clear();
    if (final) {
        state_ = State::Ground;
        overflow_ = false;
    }
    return event;
}
auto Parser::paste_chunk(bool last) -> Event
{
    auto count = std::min(buffer_.size(), options_.paste_chunk_size);
    Event event = PasteChunk{buffer_.substr(0, count), paste_first_, last,
                             paste_aborted_ && last};
    buffer_.erase(0, count);
    paste_first_ = false;
    if (last) state_ = State::Ground;
    return event;
}
auto Parser::complete_sequence() -> Event
{
    if (overflow_) return unknown(InputProblem::TooLong);
    auto bytes = std::string_view{buffer_};
    auto succeed = [&](Event event) {
        buffer_.clear();
        state_ = State::Ground;
        return event;
    };
    bool ss3 = state_ == State::Ss3;
    if (state_ == State::LegacyMouse) {
        unsigned code = static_cast<unsigned char>(bytes[3]),
                 x = static_cast<unsigned char>(bytes[4]),
                 y = static_cast<unsigned char>(bytes[5]);
        if (active_.mouse == MouseProtocol::Legacy && code >= 32 && x > 32 && y > 32)
            if (auto event = mouse_event(code - 32, x - 32, y - 32, false, false))
                return succeed(*event);
        return unknown(active_.mouse == MouseProtocol::Legacy
                           ? InputProblem::Malformed
                           : InputProblem::Unsupported);
    }
    char final = bytes.back();
    auto fields = bytes.substr(2, bytes.size() - 3);
    if (!ss3 && active_.replies && fields.starts_with('?') &&
        (final == 'u' || final == 'c' || final == 'y')) {
        if (auto reply = detail::parse_reply(fields, final))
            return succeed(TerminalReply{std::move(*reply)});
        return unknown(InputProblem::Malformed);
    }
    if (!ss3 && active_.keyboard == KeyboardProtocol::Kitty && final == 'R')
        return unknown(InputProblem::Unsupported);  // Removed by Kitty: overlaps CPR.
    if (!ss3 && active_.keyboard == KeyboardProtocol::Kitty &&
        (final == 'u' || final == '~' || final == 'A' || final == 'B' || final == 'C' ||
         final == 'D' || final == 'H' || final == 'F' || final == 'P' || final == 'Q' ||
         final == 'S' || final == 'E')) {
        if (auto key = detail::parse_kitty(fields, final))
            return succeed(std::move(*key));
        return unknown(InputProblem::Malformed);
    }
    if (!ss3 && fields.starts_with('<')) {
        if (active_.mouse != MouseProtocol::Sgr)
            return unknown(InputProblem::Unsupported);
        Parameters p{fields.substr(1)};
        if (p.valid && p.size == 3 && (final == 'M' || final == 'm'))
            if (auto event = mouse_event(p.values[0], p.values[1], p.values[2],
                                         final == 'm', true))
                return succeed(*event);
        return unknown(InputProblem::Malformed);
    }
    Parameters p{fields};
    if (!p.valid) return unknown(InputProblem::Malformed);
    if (!ss3 && final == 'M' && p.size == 3) {
        if (active_.mouse != MouseProtocol::Urxvt)
            return unknown(InputProblem::Unsupported);
        if (p.values[0] >= 32)
            if (auto event = mouse_event(p.values[0] - 32, p.values[1], p.values[2],
                                         false, false))
                return succeed(*event);
        return unknown(InputProblem::Malformed);
    }
    if (!ss3 && (final == 'I' || final == 'O') && p.size == 0) {
        if (active_.focus) return succeed(Focus{final == 'I'});
        return unknown(InputProblem::Unsupported);
    }
    if (!ss3 && final == 'Z' && p.size == 0)
        return succeed(KeyEvent{KeyCode::Tab, Modifiers{.shift = true}});
    auto key = final == '~' && !ss3 && p.size
                   ? tilde_key(p.values[0], native_function_keys_)
                   : named_key(final, ss3);
    if (!key) return unknown(InputProblem::Unsupported);
    bool valid =
        p.size <= 2 && (final == '~' ? p.size >= 1 : (p.size == 0 || p.values[0] == 1));
    if (!valid) return unknown(InputProblem::Unsupported);
    Modifiers mods{};
    if (p.size == 2) {
        if (p.values[1] < 1 || p.values[1] > 16)
            return unknown(InputProblem::Malformed);
        mods = modifiers(p.values[1]);
    }
    return succeed(KeyEvent{*key, mods});
}
auto Parser::next(std::span<char const> input) -> ParseResult
{
    if (eof_) throw std::logic_error{"Parser: reset required after finish"};
    std::size_t consumed = 0;
    auto event = [&](Event value) -> ParseResult {
        return {consumed, std::move(value)};
    };
    constexpr std::string_view end_paste = "\x1b[201~";
    while (true) {
        if (state_ == State::PasteEnding)
            return event(paste_chunk(buffer_.size() <= options_.paste_chunk_size));
        if (state_ == State::Paste &&
            buffer_.size() >= options_.paste_chunk_size + end_paste.size())
            return event(paste_chunk(false));
        if (consumed == input.size()) return {consumed, {}};
        auto byte = static_cast<unsigned char>(input[consumed]);
        if (state_ == State::Ground) {
            buffer_.clear();
            overflow_ = false;
            alt_ = false;
            if (byte == 27) {
                buffer_ += '\x1b';
                ++consumed;
                active_ = protocols_;
                state_ = State::Escape;
                continue;
            }
            if (byte < 128) {
                ++consumed;
                return event(scalar_key(byte));
            }
            utf8_width_ = utf8_width(byte);
            buffer_ += static_cast<char>(byte);
            ++consumed;
            if (!utf8_width_) return event(unknown(InputProblem::InvalidUtf8));
            state_ = State::Utf8;
            continue;
        }
        if (state_ == State::Escape) {
            if (byte == 27) {
                buffer_.clear();
                state_ = State::Ground;
                return event(scalar_key(27));
            }
            if (byte == '[' || byte == 'O' || byte == ']' || byte == 'P' ||
                byte == '_' || byte == '^') {
                buffer_ += static_cast<char>(byte);
                ++consumed;
                if (byte == '[')
                    state_ = State::Csi;
                else if (byte == 'O')
                    state_ = State::Ss3;
                else {
                    state_ = State::String;
                    string_osc_ = byte == ']';
                    string_escape_ = false;
                }
                continue;
            }
            if (byte < 128) {
                ++consumed;
                buffer_.clear();
                state_ = State::Ground;
                return event(scalar_key(byte, true));
            }
            alt_ = true;
            utf8_width_ = utf8_width(byte);
            buffer_ += static_cast<char>(byte);
            ++consumed;
            if (!utf8_width_) return event(unknown(InputProblem::InvalidUtf8));
            state_ = State::Utf8;
            continue;
        }
        if (state_ == State::Utf8) {
            if ((byte & 0xc0) != 0x80) return event(unknown(InputProblem::InvalidUtf8));
            buffer_ += static_cast<char>(byte);
            ++consumed;
            auto prefix = alt_ ? 1u : 0u;
            if (buffer_.size() == prefix + 2) {
                auto lead = static_cast<unsigned char>(buffer_[prefix]);
                if ((lead == 0xe0 && byte < 0xa0) || (lead == 0xed && byte >= 0xa0) ||
                    (lead == 0xf0 && byte < 0x90) || (lead == 0xf4 && byte >= 0x90))
                    return event(unknown(InputProblem::InvalidUtf8));
            }
            if (buffer_.size() == utf8_width_ + (alt_ ? 1 : 0)) {
                std::size_t offset = alt_ ? 1 : 0;
                try {
                    auto scalar = detail::decode_utf8(buffer_, offset);
                    auto key = scalar_key(scalar, alt_);
                    buffer_.clear();
                    state_ = State::Ground;
                    return event(key);
                }
                catch (UnicodeError const&) {
                    return event(unknown(InputProblem::InvalidUtf8));
                }
            }
            continue;
        }
        if (state_ == State::Paste) {
            // Copy ordinary runs in bulk. A possible delimiter prefix in the
            // trailing five bytes still uses the byte path below, so terminators
            // spanning calls or chunk boundaries retain exactly the same meaning.
            auto tail = std::string_view{buffer_}.substr(
                buffer_.size() > 5 ? buffer_.size() - 5 : 0);
            if (byte != 27 && input.size() - consumed >= 8 &&
                tail.find('\x1b') == tail.npos) {
                auto remaining =
                    std::string_view{input.data() + consumed, input.size() - consumed};
                auto stop = remaining.find('\x1b');
                auto count = std::min(
                    stop == remaining.npos ? remaining.size() : stop,
                    options_.paste_chunk_size + end_paste.size() - buffer_.size());
                buffer_.append(remaining.data(), count);
                consumed += count;
                continue;
            }
            buffer_ += static_cast<char>(byte);
            ++consumed;
            if (buffer_.ends_with(end_paste)) {
                buffer_.resize(buffer_.size() - end_paste.size());
                state_ = State::PasteEnding;
            }
            continue;
        }
        if (state_ != State::String && byte == 27)
            return event(unknown(
                InputProblem::Truncated));  // Leave the new introducer unconsumed.
        if (buffer_.size() == options_.sequence_limit) {
            overflow_ = true;
            return event(unknown(InputProblem::TooLong, false));
        }
        buffer_ += static_cast<char>(byte);
        ++consumed;
        if (byte == 24 || byte == 26) return event(unknown(InputProblem::Malformed));
        if (state_ == State::String) {
            bool done = (string_osc_ && byte == 7) || (string_escape_ && byte == '\\');
            string_escape_ = byte == 27;
            if (done) return event(unknown(InputProblem::Unsupported));
        }
        else if (state_ == State::LegacyMouse) {
            if (buffer_.size() == 6) return event(complete_sequence());
        }
        else if (byte >= 0x40 && byte <= 0x7e) {
            if (state_ == State::Csi && buffer_ == "\x1b[M") {
                state_ = State::LegacyMouse;
                continue;
            }
            if (state_ == State::Csi && !overflow_ && active_.bracketed_paste &&
                byte == '~') {
                Parameters params{
                    std::string_view{buffer_}.substr(2, buffer_.size() - 3)};
                if (params.valid && params.size == 1 && params.values[0] == 200) {
                    buffer_.clear();
                    state_ = State::Paste;
                    paste_first_ = true;
                    paste_aborted_ = false;
                    continue;
                }
            }
            return event(complete_sequence());
        }
        else if (byte < 0x20 || byte > 0x7e)
            return event(unknown(InputProblem::Malformed));
    }
}
auto Parser::expire() -> std::optional<Event>
{
    if (!pending_timeout()) return {};
    if (state_ == State::Escape) {
        buffer_.clear();
        state_ = State::Ground;
        return scalar_key(27);
    }
    return unknown(InputProblem::Truncated);
}
auto Parser::finish() -> std::optional<Event>
{
    eof_ = true;
    if (state_ == State::Ground) return {};
    if (state_ == State::Paste || state_ == State::PasteEnding) {
        if (state_ == State::Paste) {
            state_ = State::PasteEnding;
            paste_aborted_ = true;
        }
        return paste_chunk(buffer_.size() <= options_.paste_chunk_size);
    }
    if (state_ == State::Escape) {
        buffer_.clear();
        state_ = State::Ground;
        return scalar_key(27);
    }
    return unknown(InputProblem::Truncated);
}
}  // namespace esc
