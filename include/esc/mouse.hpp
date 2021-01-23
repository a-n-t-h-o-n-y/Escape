#ifndef ESC_MOUSE_HPP
#define ESC_MOUSE_HPP
#include <cstdint>
#include <stdexcept>
#include <string>

#include <esc/point.hpp>

namespace esc {

/// Contains mouse input device state.
struct Mouse {
    /// Standard mouse buttons to distinguish input.
    enum class Button : std::uint16_t {
        Left       = 0b00000000,
        Middle     = 0b00000001,
        Right      = 0b00000010,
        None       = 0b00000011,
        ScrollUp   = 0b01000000,
        ScrollDown = 0b01000001,
        Six        = 0b01000010,
        Seven      = 0b01000011,
        Eight      = 0b10000000,
        Nine       = 0b10000001,
        Ten        = 0b10000010,
        Eleven     = 0b10000011,
    };

    /// The terminal screen coordinates of the input event.
    Point at;

    /// The mouse button that created the event.
    Button button;

    /// Keyboard modifiers
    struct Modifiers {
        bool shift = false;
        bool ctrl  = false;
        bool alt   = false;
    } modifiers = {};
};

/// Enum to string representation for Mouse::Button.
[[nodiscard]] inline auto to_string(Mouse::Button b) -> std::string
{
    auto const btn = std::string{"Mouse::Button::"};
    switch (b) {
        case Mouse::Button::Left: return btn + "Left";
        case Mouse::Button::Middle: return btn + "Middle";
        case Mouse::Button::Right: return btn + "Right";
        case Mouse::Button::None: return btn + "None";
        case Mouse::Button::ScrollUp: return btn + "ScrollUp";
        case Mouse::Button::ScrollDown: return btn + "ScrollDown";
        case Mouse::Button::Six: return btn + "Six";
        case Mouse::Button::Seven: return btn + "Seven";
        case Mouse::Button::Eight: return btn + "Eight";
        case Mouse::Button::Nine: return btn + "Nine";
        case Mouse::Button::Ten: return btn + "Ten";
        case Mouse::Button::Eleven: return btn + "Eleven";
    }
    throw std::runtime_error{"Invalid Mouse::Button enum value: " +
                             std::to_string(static_cast<std::uint16_t>(b))};
}

}  // namespace esc
#endif  // ESC_MOUSE_HPP
