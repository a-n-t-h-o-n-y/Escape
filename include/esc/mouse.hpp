#ifndef ESC_MOUSE_HPP
#define ESC_MOUSE_HPP
#include <cstdint>
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
[[nodiscard]] auto to_string(Mouse::Button b) -> std::string;

/// Defines all input modes for the Mouse.
enum class Mouse_mode {
    Off,    // No Mouse Events are generated, this is the default.
    Basic,  // Mouse Press & Release Events for all Buttons and Scroll Wheel.
    Drag,   // Basic, plus Move Events while a Button is pressed.
    Move    // Basic, plus Move Events sent with or without a Button pressed.
};

/// Return a string display of the given Mouse_mode enum.
[[nodiscard]] auto to_string(Mouse_mode mm) -> std::string;

}  // namespace esc
#endif  // ESC_MOUSE_HPP
