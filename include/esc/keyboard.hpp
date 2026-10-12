#pragma once
#include <cstdint>
#include <esc/key.hpp>
#include <optional>
#include <string>
namespace esc {
enum class KeyboardProtocol { Legacy, Kitty };
enum class KittyFlags : unsigned {
    Disambiguate = 1,
    EventTypes = 2,
    AlternateKeys = 4,
    AllKeys = 8,
    AssociatedText = 16
};
constexpr auto operator|(KittyFlags a, KittyFlags b) -> KittyFlags
{ return static_cast<KittyFlags>(static_cast<unsigned>(a) | static_cast<unsigned>(b)); }
enum class KeyAction { Press, Repeat, Release };
// A text-only IME event has no key. Legacy input still produces KeyPress.
struct KeyEvent {
    std::optional<Key> key;
    Modifiers modifiers = {};
    KeyAction action = KeyAction::Press;
    std::optional<char32_t> shifted_key, base_layout_key;
    std::u32string text;
    friend auto operator==(KeyEvent const&, KeyEvent const&) -> bool = default;
};
}  // namespace esc
