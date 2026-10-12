#include <array>
#include <charconv>
#include <esc/detail/unicode.hpp>
#include "detail/protocol.hpp"

namespace esc::detail {
namespace {
auto number(std::string_view text, unsigned& value) -> bool
{
    if (text.empty()) return false;
    auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    return error == std::errc{} && end == text.data() + text.size();
}

template <std::size_t N>
auto split(std::string_view text,
           char delimiter,
           std::array<std::string_view, N>& output) -> unsigned
{
    unsigned count = 0;
    while (true) {
        if (count == N) return 0;
        auto at = text.find(delimiter);
        output[count++] = text.substr(0, at);
        if (at == text.npos) return count;
        text.remove_prefix(at + 1);
    }
}

constexpr KeyCode functional[] = {
    KeyCode::CapsLock,
    KeyCode::ScrollLock,
    KeyCode::NumLock,
    KeyCode::PrintScreen,
    KeyCode::Pause,
    KeyCode::Menu,
    KeyCode::F1,
    KeyCode::F2,
    KeyCode::F3,
    KeyCode::F4,
    KeyCode::F5,
    KeyCode::F6,
    KeyCode::F7,
    KeyCode::F8,
    KeyCode::F9,
    KeyCode::F10,
    KeyCode::F11,
    KeyCode::F12,
    KeyCode::F13,
    KeyCode::F14,
    KeyCode::F15,
    KeyCode::F16,
    KeyCode::F17,
    KeyCode::F18,
    KeyCode::F19,
    KeyCode::F20,
    KeyCode::F21,
    KeyCode::F22,
    KeyCode::F23,
    KeyCode::F24,
    KeyCode::F25,
    KeyCode::F26,
    KeyCode::F27,
    KeyCode::F28,
    KeyCode::F29,
    KeyCode::F30,
    KeyCode::F31,
    KeyCode::F32,
    KeyCode::F33,
    KeyCode::F34,
    KeyCode::F35,
    KeyCode::Keypad0,
    KeyCode::Keypad1,
    KeyCode::Keypad2,
    KeyCode::Keypad3,
    KeyCode::Keypad4,
    KeyCode::Keypad5,
    KeyCode::Keypad6,
    KeyCode::Keypad7,
    KeyCode::Keypad8,
    KeyCode::Keypad9,
    KeyCode::KeypadDecimal,
    KeyCode::KeypadDivide,
    KeyCode::KeypadMultiply,
    KeyCode::KeypadMinus,
    KeyCode::KeypadPlus,
    KeyCode::KeypadEnter,
    KeyCode::KeypadEqual,
    KeyCode::KeypadSeparator,
    KeyCode::KeypadLeft,
    KeyCode::KeypadRight,
    KeyCode::KeypadUp,
    KeyCode::KeypadDown,
    KeyCode::KeypadPageUp,
    KeyCode::KeypadPageDown,
    KeyCode::KeypadHome,
    KeyCode::KeypadEnd,
    KeyCode::KeypadInsert,
    KeyCode::KeypadDelete,
    KeyCode::KeypadBegin,
    KeyCode::MediaPlay,
    KeyCode::MediaPause,
    KeyCode::MediaPlayPause,
    KeyCode::MediaReverse,
    KeyCode::MediaStop,
    KeyCode::MediaFastForward,
    KeyCode::MediaRewind,
    KeyCode::MediaTrackNext,
    KeyCode::MediaTrackPrevious,
    KeyCode::MediaRecord,
    KeyCode::VolumeDown,
    KeyCode::VolumeUp,
    KeyCode::VolumeMute,
    KeyCode::LeftShift,
    KeyCode::LeftControl,
    KeyCode::LeftAlt,
    KeyCode::LeftSuper,
    KeyCode::LeftHyper,
    KeyCode::LeftMeta,
    KeyCode::RightShift,
    KeyCode::RightControl,
    KeyCode::RightAlt,
    KeyCode::RightSuper,
    KeyCode::RightHyper,
    KeyCode::RightMeta,
    KeyCode::IsoLevel3Shift,
    KeyCode::IsoLevel5Shift,
};

auto identity(unsigned value) -> std::optional<Key>
{
    switch (value) {
        case 9: return KeyCode::Tab;
        case 13: return KeyCode::Enter;
        case 27: return KeyCode::Escape;
        case 127: return KeyCode::Backspace;
    }
    if (value >= 57358 && value <= 57454) return functional[value - 57358];
    if (value >= 57344 && value <= 63743) return KittyKey{value};
    if (value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) return {};
    return Key{static_cast<char32_t>(value)};
}

auto legacy_identity(unsigned number, char final) -> std::optional<Key>
{
    if (final == '~') {
        switch (number) {
            case 2: return KeyCode::Insert;
            case 3: return KeyCode::Delete;
            case 5: return KeyCode::PageUp;
            case 6: return KeyCode::PageDown;
            case 7: return KeyCode::Home;
            case 8: return KeyCode::End;
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
            case 57427: return KeyCode::KeypadBegin;
        }
        return {};
    }
    if (number != 1) return {};
    switch (final) {
        case 'A': return KeyCode::ArrowUp;
        case 'B': return KeyCode::ArrowDown;
        case 'C': return KeyCode::ArrowRight;
        case 'D': return KeyCode::ArrowLeft;
        case 'H': return KeyCode::Home;
        case 'F': return KeyCode::End;
        case 'P': return KeyCode::F1;
        case 'Q': return KeyCode::F2;
        case 'S': return KeyCode::F4;
        case 'E': return KeyCode::KeypadBegin;
    }
    return {};
}
}  // namespace

void validate_kitty_flags(KittyFlags flags)
{
    auto bits = static_cast<unsigned>(flags);
    if (!bits || (bits & ~31u) || ((bits & 16u) && !(bits & 8u)))
        throw std::invalid_argument{
            "Kitty: invalid flags; associated text requires all keys"};
}

auto parse_kitty(std::string_view fields, char final) -> std::optional<KeyEvent>
{
    std::array<std::string_view, 3> parameters{}, keys{};
    std::array<std::string_view, 2> mods{};
    auto count = split(fields, ';', parameters);
    if (!count || (final != 'u' && count > 2)) return {};
    auto key_count = split(parameters[0], ':', keys);
    auto mod_count = split(parameters[1], ':', mods);
    if (!key_count || !mod_count || (final != 'u' && key_count != 1)) return {};
    unsigned code = 1, modifier = 1, action = 1;
    if (!parameters[0].empty()) {
        if (!number(keys[0], code)) return {};
    }
    else if (final == 'u' || final == '~')
        return {};
    if (!mods[0].empty() && !number(mods[0], modifier)) return {};
    if (mod_count == 2 && !mods[1].empty() && !number(mods[1], action)) return {};
    if (!modifier || modifier > 256 || !action || action > 3) return {};
    auto bits = modifier - 1;
    KeyEvent result;
    if (code || final != 'u') {
        result.key = final == 'u' ? identity(code) : legacy_identity(code, final);
        if (!result.key) return {};
    }
    result.modifiers = {bool(bits & 1),  bool(bits & 4),  bool(bits & 2),
                        bool(bits & 32), bool(bits & 8),  bool(bits & 16),
                        bool(bits & 64), bool(bits & 128)};
    result.action = static_cast<KeyAction>(action - 1);
    auto alternate = [&](std::string_view value, std::optional<char32_t>& destination) {
        if (value.empty()) return true;
        unsigned scalar = 0;
        if (!number(value, scalar) || scalar > 0x10ffff ||
            (scalar >= 0xd800 && scalar <= 0xdfff))
            return false;
        destination = static_cast<char32_t>(scalar);
        return true;
    };
    if (!alternate(keys[1], result.shifted_key) ||
        !alternate(keys[2], result.base_layout_key) ||
        (result.shifted_key && !result.modifiers.shift))
        return {};
    auto text = parameters[2];
    while (!text.empty()) {
        auto at = text.find(':');
        unsigned scalar = 0;
        if (!number(text.substr(0, at), scalar) || scalar < 32 ||
            (scalar >= 127 && scalar <= 159) || scalar > 0x10ffff ||
            (scalar >= 0xd800 && scalar <= 0xdfff))
            return {};
        result.text += static_cast<char32_t>(scalar);
        if (at == text.npos) break;
        text.remove_prefix(at + 1);
        if (text.empty()) return {};
    }
    if (!result.key && result.text.empty()) return {};
    return result;
}
}  // namespace esc::detail
