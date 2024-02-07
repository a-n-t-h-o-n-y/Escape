#pragma once

#include <utility>

#include <esc/area.hpp>
#include <esc/detail/any_of.hpp>
#include <esc/mouse.hpp>

/** @file
 *  Terminal Specific Settings.
 */

namespace esc {

// ECHO ------------------------------------------------------------------------

/**
 * `On` will print keypresses to the screen; `Off` will not.
 */
enum class Echo : bool { On, Off };

/**
 * Set whether the terminal echos input text or not.
 * @param x The Echo value to set.
 */
void set(Echo x);

// INPUT BUFFER ----------------------------------------------------------------

/**
 * `Canonical` will buffer input and wait to send it until 'Enter' is pressed;
 * `Immediate` sends every input byte immediatly to your application.
 */
enum class InputBuffer : bool { Canonical, Immediate };

/**
 * Sest the input buffer mode.
 * @param x The InputBuffer value to set.
 */
void set(InputBuffer x);

// SIGNALS ---------------------------------------------------------------------

/**
 * `On` will generate signals from ctrl-[key] presses; `Off` will not.
 */
enum class Signals : bool { On, Off };

/**
 * Set whether or not signals are generated from ctrl-[key] presses.
 *
 * @details With Signals off, all ctrl-[key] presses generate the value of the
 *          letter in the alphabet that was pressed. ctrl-a return 1, ctrl-z
 *          returns 26, etc...
 * @param   x The Signals value to set.
 */
void set(Signals x);

// SCREEN BUFFER ---------------------------------------------------------------

/**
 * `Normal` will display the existing terminal screen; `Alternate` will display
 * a blank screen.
 */
enum class ScreenBuffer { Normal, Alternate };

/**
 * Set the screen buffer to the given value.
 * @details Calls on write internally, but does not call flush().
 * @param   x The ScreenBuffer value to set.
 */
void set(ScreenBuffer x);

// KEY RELEASE ---------------------------------------------------------------

/**
 * `Normal` will generate KeyPress Events and auto-repeat if key is held down;
 * `Raw` will generate KeyPress and KeyRelease Events, the shift key is not
 * applied with other keys, each key press and release is its own event, all
 * letters are lowercase.
 *
 * @details `Raw` mode is experimental typically requires superuser privileges,
 * should be avoided.
 */
enum class KeyMode : bool { Normal, Raw };

/**
 * Set the key mode to the given value.
 * @details Calls on ioctl internally.
 * @param   x The KeyMode value to set.
 */
void set(KeyMode x);

// CURSOR ----------------------------------------------------------------------

/**
 * `Show` will display the cursor on screen; `Hide` will not.
 */
enum class Cursor { Show, Hide };

/**
 * Set the cursor to the given value.
 * @details Calls on write internally, but does not call flush().
 * @param   x The Cursor value to set.
 */
void set(Cursor x);

// MOUSE MODE ------------------------------------------------------------------

/**
 * Set the mouse mode to the given value.
 * @details Calls on write internally, but does not call flush().
 * @param   x The MouseMode value to set.
 */
void set(MouseMode x);

// CONVENIENCE -----------------------------------------------------------------

/**
 * Types that represent a setting on the terminal.
 */
template <typename T>
concept Setable = detail::AnyOf<T,
                                Echo,
                                InputBuffer,
                                Signals,
                                ScreenBuffer,
                                Cursor,
                                MouseMode,
                                KeyMode>;

/**
 * Convenience function to set multiple properties at once.
 * @tparam Args The types of the properties to set.
 * @param  args The properties to set.
 */
template <Setable... Args>
auto set(Args&&... args) -> void
{
    static_assert(sizeof...(Args) > 0,
                  "set(...): Must have at least one argument.");
    (set(std::forward<Args>(args)), ...);
}

// INITIALIZE ------------------------------------------------------------------

/**
 * Prepare the terminal for input/output and display with the given settings.
 *
 * @details Call this function before performing any calls to write(), and call
 *          uninitialize_terminal() on application exit. Installs sigterm
 *          handler to uninitialize the terminal, if sigint_uninit is true.
 *
 * @param ScreenBuffer
 *     Alternate: Provides a blank terminal screen.
 *     Normal:    Provides the existing terminal display.
 *
 *     The Normal buffer can be set at shutdown after using the Alternate buffer
 *     in order to restore the terminal screen to how it was before the
 *     application started.
 *
 * @param MouseMode
 *     Off:   Generates no Mouse Events.
 *     Basic: Generate Mouse Press and Release Events for all buttons and the
 *            scroll wheel.
 *     Drag:  Basic, plus Mouse Move Events while a button is pressed.
 *     Move:  Basic, plus Mouse Move Events are generated with or without a
 *            button pressed.
 *
 * @param Cursor
 *     Show: The Cursor will be displayed on screen.
 *     Hide: The Cursor will not be displayed on screen.
 *
 * @param Echo
 *     On:  Text characters will immediatly appear on screen
 *          at the cursor position on key presses.
 *     Off: Text will not be automatically displayed on key press.
 *
 * @param InputBuffer
 *     Canonical: Keyboard input will be buffered until the 'Enter' key is
 *                pressed, or the buffer is filled.
 *     Immediate: Each input event is immediately sent readable.
 *
 * @param Signals
 *     On:  Signals can be generated from ctrl-[key] presses, for instance
 *          ctrl-c will send SIGINT instead of byte 3.
 *     Off: Signals will not be generated on ctrl-[key] presses, sending the
 *          byte value of the ctrl character instead.
 *
 * @param KeyMode
 *     Normal: KeyPress Events generated and auto-repeated if key is held down.
 *     Raw:    KeyPress and KeyRelease Events are generated, the shift key is
 *             not applied with other keys, each key press and release is its
 *             own event, all letters are lowercase.
 */
void initialize_terminal(ScreenBuffer,
                         MouseMode,
                         Cursor,
                         Echo,
                         InputBuffer,
                         Signals,
                         KeyMode,
                         bool sigint_uninit = true);

/**
 * Initialize the terminal with 'normal' settings.
 *
 * @details This is for typical console applications, the normal screen buffer
 *          is set, key press echo is on, and the input buffer is canonical.
 */
void initialize_normal_terminal();

/**
 * Initialize the terminal with 'interactive' settings.
 *
 * @details This is for typical console full-screen applications, the alternate
 *          screen buffer is set, key press echo is off, and the input buffer is
 *          immediate.
 */
void initialize_interactive_terminal(MouseMode mouse_mode = MouseMode::Basic,
                                     KeyMode key_mode     = KeyMode::Normal,
                                     Signals signals      = Signals::On);

// UNINITIALIZE ----------------------------------------------------------------

/**
 * Restore terminal state to before any initialize...() functions were called.
 *
 * @details This resets Input/Output settings. This also sets the normal screen
 *          buffer, displays the cursor, and disables mouse input.
 */
void uninitialize_terminal();

// QUERY -----------------------------------------------------------------------

/**
 * Get the width of the terminal screen.
 * @details Uses ioctl to get the terminal width.
 * @return The width of the terminal screen.
 * @throws std::runtime_error if ioctl fails.
 */
[[nodiscard]] auto terminal_width() -> int;

/**
 * Get the height of the terminal screen.
 * @details Uses ioctl to get the terminal height.
 * @return The height of the terminal screen.
 * @throws std::runtime_error if ioctl fails.
 */
[[nodiscard]] auto terminal_height() -> int;

/**
 * Get the width and height of the terminal screen.
 * @details Uses ioctl to get the terminal width and height.
 * @return The Area (width and height) of the terminal screen.
 * @throws std::runtime_error if ioctl fails.
 */
[[nodiscard]] auto terminal_area() -> Area;

}  // namespace esc