#ifndef ESC_TERMINAL_HPP
#define ESC_TERMINAL_HPP
#include <esc/area.hpp>
#include <esc/detail/is_any_of.hpp>
#include <esc/mouse.hpp>

/** \file
 *  Terminal Specific Settings. */

namespace esc {

// ECHO ------------------------------------------------------------------------

/// Keypresses are immediately printed to the screen.
enum class Echo : bool { On, Off };

/// Set whether the terminal echos input text or not.
void set(Echo);

// INPUT BUFFER ----------------------------------------------------------------

/// Canonical will buffer input and wait to send it until 'Enter' is pressed.
/** Immediate sends every input byte immediatly to your application. */
enum class Input_buffer : bool { Canonical, Immediate };

/// Set whether the terminal is in Canonical or Immediate mode.
void set(Input_buffer);

// SIGNALS ---------------------------------------------------------------------

/// Signals generated from ctrl-[key] presses.
enum class Signals : bool { On, Off };

/// Set whether or not signals are generated from ctrl-[key] presses.
/** With Signals off, all ctrl-[key] presses generate the value of the letter in
 *  the alphabet that was pressed. ctrl-a return 1, ctrl-z returns 26, etc... */
void set(Signals);

// SCREEN BUFFER ---------------------------------------------------------------

/// Types of screen buffers.
/** Alternate screen buffer starts as a blank screen. */
enum class Screen_buffer { Normal, Alternate };

/// Enable the given screen buffer type.
/** Calls on write internally, but does not call flush(). */
void set(Screen_buffer);

// KEY RELEASE ---------------------------------------------------------------

/// Raw key mode enables Key_release Events and does not combine shift with key.
/** Raw mode makes the read() call more complex, only enable if needed. All keys
 *  returned in Raw mode are lower-case. Has only been tested on a single laptop
 *  keyboard. */
enum class Key_mode : bool { Normal, Raw };

/// Enable the given Key_mode value.
/** Calls on ioctl internally. */
void set(Key_mode);

// CURSOR ----------------------------------------------------------------------

/// Types of Cursor display.
enum class Cursor { Show, Hide };

/// Enable the given cursor display type.
/** Calls on write internally, but does not call flush(). */
void set(Cursor);

// MOUSE MODE ------------------------------------------------------------------

/// Set the mouse input mode; determines which Events are generated for mouse.
/** Calls on write internally, but does not call flush(). */
void set(Mouse_mode);

// CONVENIENCE -----------------------------------------------------------------

template <typename T>
bool constexpr is_setable = detail::is_any_of<T,
                                              Echo,
                                              Input_buffer,
                                              Signals,
                                              Screen_buffer,
                                              Cursor,
                                              Mouse_mode,
                                              Key_mode>;

/// Convenience function to set multiple properties at once.
template <typename... Args>
void set(Args... args)
{
    static_assert(sizeof...(Args) > 0,
                  "set(...): Must have at least one argument.");
    static_assert((is_setable<Args> && ...),
                  "set(...): All Args... must be setable types.");
    (set(args), ...);
}

// INITIALIZE ------------------------------------------------------------------

/// Prepare the terminal for input/output and display with the given settings.
/** Call this function before performing any calls to write(), and call
 *  uninitialize_terminal() on application exit.
 *
 *  Screen_buffer - Alternate: Provides a blank terminal screen.
 *                  Normal:    Provides the existing terminal display.
 *                  The Normal buffer can be set at shutdown after using the
 *                  Alternate buffer in order to restore the terminal screen to
 *                  how it was before the application started.
 *
 *  Mouse_mode  - - Off:   Generates no Mouse Events.
 *                  Basic: Generate Mouse Press and Release Events for all
 *                         buttons and the scroll wheel.
 *                  Drag:  Basic, plus Mouse Move Events while a button is
 *                         pressed.
 *                  Move:  Basic, plus Mouse Move Events are generated with or
 *                         without a button pressed.
 *
 *  Cursor  - - - - Show: The Cursor will be displayed on screen.
 *                  Hide: The Cursor will not be displayed on screen.
 *
 *  Echo  - - - - - On:   Text characters will immediatly appear on screen at
 *                        the cursor position on key presses.
 *                  Off:  Text will not be automatically displayed on key press.
 *
 *  Input_buffer  - Canonical: Keyboard input will be buffered until the 'Enter'
 *                             key is pressed, or the buffer is filled.
 *                  Immediate: Each input event is immediately sent readable.
 *
 *  Signals - - - - On:  Signals can be generated from ctrl-[key] presses, for
 *                       instance ctrl-c will send SIGINT instead of byte 3.
 *                  Off: Signals will not be generated on ctrl-[key] presses,
 *                       sending the byte value of the ctrl character instead.
 *
 *  Key_mode  - - - Normal: Key_press Events generated and auto-repeated if key
 *                          is held down.
 *                  Raw:    Key_press and Key_release Events are generated, the
 *                          shift key is not applied with other keys, each key
 *                          press and release is its own event. Useful for games
 *                          and where you need to keep track of multiple keys
 *                          held down at once. All keys returned in Raw mode are
 *                          lower-case. Has only been tested on a single laptop
 *                          keyboard.
 */
void initialize_terminal(Screen_buffer,
                         Mouse_mode,
                         Cursor,
                         Echo,
                         Input_buffer,
                         Signals,
                         Key_mode);

/// Initialize the terminal with 'normal' settings.
/** The terminal should mimic normal terminal input/output activity, leaving the
 *  screen buffer as is and echoing key presses, among other things. */
void initialize_normal_terminal();

/// Initialize the terminal with 'interactive' settings.
/** This is for typical console full-screen applications, the alternate screen
 *  buffer is set, key press echo is off, and the input buffer is immediate. */
void initialize_interactive_terminal(Mouse_mode mouse_mode = Mouse_mode::Basic,
                                     Key_mode key_mode     = Key_mode::Normal,
                                     Signals signals       = Signals::On);

// UNINITIALIZE ----------------------------------------------------------------

/// Restore terminal state to before any initialize...() functions were called.
/** This is Input/Output settings and the C locale. This also sets the normal
 *  screen buffer, displays the cursor, and disables mouse input. */
void uninitialize_terminal();

// QUERY -----------------------------------------------------------------------

/// Return the width of the terminal screen.
auto terminal_width() -> int;

/// Return the height of the terminal screen.
auto terminal_height() -> int;

/// Return the width and height of the terminal screen.
[[nodiscard]] auto terminal_area() -> Area;

}  // namespace esc
#endif  // ESC_TERMINAL_HPP
