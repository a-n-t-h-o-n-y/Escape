#pragma once

#include <cstdint>
#include <esc/key.hpp>
#include <optional>
#include <string>

namespace esc {

/// Grammar used to interpret keyboard input.
enum class KeyboardProtocol { Legacy, Kitty };

/// Requested Kitty keyboard enhancements; combine with operator|.
enum class KittyFlags : unsigned {
    Disambiguate = 1,
    EventTypes = 2,
    AlternateKeys = 4,
    AllKeys = 8,
    AssociatedText = 16
};

constexpr auto operator|(KittyFlags a, KittyFlags b) -> KittyFlags
{ return static_cast<KittyFlags>(static_cast<unsigned>(a) | static_cast<unsigned>(b)); }

/// Press, repeat, or release, where reported by the input protocol.
enum class KeyAction { Press, Repeat, Release };

/// Legacy and enhanced protocols share this event. Unreported text stays empty.
/// A text-only IME event has no key.
struct KeyEvent {
    std::optional<Key> key = {};
    Modifiers modifiers = {};
    KeyAction action = KeyAction::Press;
    std::optional<char32_t> shifted_key = {}, base_layout_key = {};
    std::u32string text = {};

    /// Match the key, exact modifiers, and action. Defaults to an unmodified press.
    [[nodiscard]] auto matches(Key const& expected,
                               Modifiers required = {},
                               KeyAction expected_action = KeyAction::Press) const
        -> bool
    {
        return key && *key == expected && modifiers == required &&
               action == expected_action;
    }

    friend auto operator==(KeyEvent const&, KeyEvent const&) -> bool = default;
};
}  // namespace esc
