#pragma once
#include <optional>
#include <variant>

namespace esc {
enum class KeyCode {
    Escape,
    Enter,
    Tab,
    Backspace,
    ArrowUp,
    ArrowDown,
    ArrowLeft,
    ArrowRight,
    Home,
    End,
    Insert,
    Delete,
    PageUp,
    PageDown,
    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12,
    F13,
    F14,
    F15,
    F16,
    F17,
    F18,
    F19,
    F20,
    F21,
    F22,
    F23,
    F24,
    KeypadEnter,
    Keypad0,
    Keypad1,
    Keypad2,
    Keypad3,
    Keypad4,
    Keypad5,
    Keypad6,
    Keypad7,
    Keypad8,
    Keypad9,
    KeypadDecimal,
    KeypadPlus,
    KeypadMinus,
    KeypadMultiply,
    KeypadDivide
};
using Key = std::variant<char32_t, KeyCode>;
struct Modifiers {
    bool shift = false, ctrl = false, alt = false, meta = false;
    friend auto operator==(Modifiers const&, Modifiers const&) -> bool = default;
};
[[nodiscard]] inline auto text_scalar(Key const& key) noexcept
    -> std::optional<char32_t>
{
    if (auto scalar = std::get_if<char32_t>(&key)) return *scalar;
    return std::nullopt;
}
}  // namespace esc
