#pragma once

#include <cstdint>
#include <string>

#include <esc/point.hpp>

namespace esc {

/**
 * Represents the state of the mouse input device.
 */
struct Mouse {
    /**
     * Standard mouse buttons.
     */
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

    /**
     * Where the event took place on the terminal screen.
     */
    Point at;

    /**
     * The button that was pressed or released to create the event.
     */
    Button button;

    /**
     * Keyboard modifiers that were active when the event was generated.
     */
    struct Modifiers {
        bool shift = false;
        bool ctrl  = false;
        bool alt   = false;
    } modifiers = {};
};

/**
 * Return a string display of the given Mouse::Button enum.
 *
 * @param b The Mouse::Button to convert to a string.
 * @return A string representation of the given Mouse::Button.
 * @throws std::runtime_error if the given Mouse::Button is not a valid enum.
 */
[[nodiscard]] auto to_string(Mouse::Button b) -> std::string;

/**
 * Input modes for the Mouse.
 *
 * @details Off: No Mouse Events are generated.
 * @details Basic: Mouse Press & Release Events for all Buttons + Scroll Wheel.
 * @details Drag: Basic, plus Move Events while a Button is pressed.
 * @details Move: Basic, plus Move Events sent with or without a Button pressed.
 */
enum class MouseMode { Off, Basic, Drag, Move };

/**
 * Return a string display of the given MouseMode enum.
 *
 * @param mm The MouseMode to convert to a string.
 * @return A string representation of the given MouseMode.
 * @throws std::runtime_error if the given MouseMode is not a valid enum.
 */
[[nodiscard]] auto to_string(MouseMode mm) -> std::string;

}  // namespace esc