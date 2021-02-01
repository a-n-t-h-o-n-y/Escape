#ifndef ESC_TERMINAL_HPP
#define ESC_TERMINAL_HPP

namespace esc {

/// Keypresses are immediately printed to the screen.
enum class Echo : bool { On, Off };

/// Canonical will buffer input and wait to send it until 'Enter' is pressed.
/** Immediate sends every input byte immediatly to your application. */
enum class Mode : bool { Canonical, Immediate };

/// Signals generated from ctrl-[key] presses.
enum class Signals : bool { On, Off };

/// Set whether the terminal echos input text or not.
void set_echo(Echo e);

/// Set whether the terminal is in Canonical or Immediate mode.
void set_mode(Mode m);

/// Set whether or not signals are generated from ctrl-[key] presses.
/** With Signals off, all ctrl-[key] presses generate the value of the letter in
 *  the alphabet that was pressed. ctrl-a return 1, ctrl-z returns 26, etc... */
void set_signals(Signals s);

/// Prepare the terminal for input/output with the given settings.
void initialize_terminal(Echo e, Mode m, Signals s);

/// Restore terminal state to before initialize_terminal() was called.
/** This is Input/Output settings and the C locale. This does not restore the
 *  screen display, if alternate_screen_buffer() was used, the normal screen
 *  buffer has to be set manually. */
void uninitialize_terminal();

}  // namespace esc
#endif  // ESC_TERMINAL_HPP
