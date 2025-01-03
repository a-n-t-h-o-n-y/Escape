#include <esc/terminal.hpp>

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <esc/detail/console_file.hpp>
#include <esc/detail/is_urxvt.hpp>
#include <esc/detail/signals.hpp>
#include <esc/detail/tty_file.hpp>
#include <esc/io.hpp>
#include <esc/mouse.hpp>
#include <esc/terminfo.hpp>

namespace {

[[nodiscard]] auto current_termios() -> ::termios
{
    auto current = ::termios{};
    ::tcgetattr(STDIN_FILENO, &current);
    return current;
}

void fix_ctrl_m()
{
    auto settings = current_termios();
    settings.c_iflag &= ~(ICRNL);
    ::tcsetattr(STDIN_FILENO, TCSAFLUSH, &settings);
}

[[nodiscard]] auto turn_off_auto_wrap() -> std::string { return "\033[?7l"; }

[[nodiscard]] auto turn_on_auto_wrap() -> std::string { return "\033[?7h"; }

auto original_termios = ::termios{};

}  // namespace

namespace esc {

void set(Echo x)
{
    auto const echo_flag = (x == Echo::On) ? ECHO : ~ECHO;

    auto settings = current_termios();
    settings.c_lflag &= echo_flag;
    ::tcsetattr(STDIN_FILENO, TCSAFLUSH, &settings);
}

void set(InputBuffer x)
{
    auto const canon_flag = (x == InputBuffer::Canonical) ? ICANON : ~(ICANON);

    auto settings = current_termios();
    settings.c_lflag &= canon_flag;
    if (x == InputBuffer::Immediate) {
        settings.c_cc[VMIN] = 1;  // Min. number of bytes before sending.
    }
    ::tcsetattr(STDIN_FILENO, TCSAFLUSH, &settings);
}

void set(Signals x)
{
    // ctrl-c, ctrl-z, ctrl-s, ctrl-q, ctrl-v all send their ctrl byte value rather than
    // changing the terminal's behaviour.
    auto const [lflags, iflags] = [x] {
        auto const lflags = ISIG | IEXTEN;
        auto const iflags = IXON;
        return (x == Signals::On) ? std::pair{lflags, iflags}
                                  : std::pair{~lflags, ~iflags};
    }();

    auto settings = current_termios();
    settings.c_lflag &= lflags;
    settings.c_iflag &= iflags;
    ::tcsetattr(STDIN_FILENO, TCSAFLUSH, &settings);
}

void set(ScreenBuffer x)
{
    switch (x) {
        case ScreenBuffer::Normal:
            write(
                "\033["
                "?1049l");
            break;
        case ScreenBuffer::Alternate:
            write(
                "\033["
                "?1049h");
            break;
    }
}

void set(KeyMode x)
{
    // TODO implementation is messy, depends on call order.

    // from linux/kd.h
    constexpr auto k_xlate = 0x01;
    constexpr auto k_raw = 0x00;
    switch (x) {
        case KeyMode::Normal:
            if (detail::tty_file_descriptor.has_value()) {
                detail::set_keyboard_mode(*detail::tty_file_descriptor, k_xlate);
            }
            break;
        case KeyMode::Raw:
            detail::tty_file_descriptor = detail::open_console_file();
            detail::set_keyboard_mode(*detail::tty_file_descriptor, k_raw);
            break;
    }
}

void set(CursorMode x)
{
    switch (x) {
        case CursorMode::Show:
            write(
                "\033["
                "?25h");
            break;
        case CursorMode::Hide:
            write(
                "\033["
                "?25l");
            break;
    }
}

void set(MouseMode x)
{
    auto const ext_mode = detail::is_urxvt(TERM_var()) ? "1015" : "1006";

    auto result = std::string{};
    switch (x) {
        case MouseMode::Off:
            result.append("\033[?1000;1002;1003;").append(ext_mode).append("l");
            break;
        case MouseMode::Basic:
            result.append("\033[?1002;1003l");  // Off
            result.append("\033[?1000;").append(ext_mode).append("h");
            break;
        case MouseMode::Drag:
            result.append("\033[?1000;1003l");  // Off
            result.append("\033[?1002;").append(ext_mode).append("h");
            break;
        case MouseMode::Move:
            result.append("\033[?1000;1002l");  // Off
            result.append("\033[?1003;").append(ext_mode).append("h");
            break;
    }
    write(result);
}

void initialize_terminal(ScreenBuffer screen_buffer,
                         MouseMode mouse_mode,
                         CursorMode cursor,
                         Echo echo,
                         InputBuffer input_buffer,
                         Signals signals,
                         KeyMode key_mode,
                         bool sigint_uninit)
{
    auto constexpr stdout_buf_size = 4'096;
    // TODO record current settings before calling set, this is ioctl things and
    // tcsetaddr things. termios? probably from tcsetaddr

    original_termios = current_termios();

    std::setvbuf(stdout, nullptr, _IOFBF, stdout_buf_size);
    detail::register_signals(sigint_uninit);

    fix_ctrl_m();
    write(turn_off_auto_wrap());

    try {
        set(echo, input_buffer, signals, screen_buffer, mouse_mode, cursor, key_mode);
    }
    catch (std::runtime_error const& e) {
        uninitialize_terminal();
        std::cerr << "Error: " << e.what() << '\n';
        std::exit(1);
    }
    flush();
}

void initialize_normal_terminal()
{
    initialize_terminal(ScreenBuffer::Normal, MouseMode::Off, CursorMode::Show,
                        Echo::On, InputBuffer::Canonical, Signals::On, KeyMode::Normal);
}

void initialize_interactive_terminal(MouseMode mouse_mode,
                                     KeyMode key_mode,
                                     Signals signals)
{
    initialize_terminal(ScreenBuffer::Alternate, mouse_mode, CursorMode::Hide,
                        Echo::Off, InputBuffer::Immediate, signals, key_mode);
}

void uninitialize_terminal()
{
    // TODO take settings parameter and use that to reset the terminal to settings
    // before.
    write(turn_on_auto_wrap());
    set(ScreenBuffer::Normal, MouseMode::Off, CursorMode::Show, KeyMode::Normal);
    flush();
    ::tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios);
    if (detail::tty_file_descriptor.has_value()) {
        close(detail::tty_file_descriptor.value());
    }
}

auto terminal_width() -> int
{
    auto w = ::winsize{};
    auto const result = ::ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    if (result == -1) {
        throw std::runtime_error{
            "terminal.cpp terminal_width(): Can't Read Window Size."};
    }
    return w.ws_col;
}

auto terminal_height() -> int
{
    auto w = ::winsize{};
    auto const result = ::ioctl(STDIN_FILENO, TIOCGWINSZ, &w);
    if (result == -1) {
        throw std::runtime_error{
            "terminal.cpp terminal_height(): Can't Read Window Size."};
    }
    return w.ws_row;
}

auto terminal_area() -> Area
{
    return {
        .width = terminal_width(),
        .height = terminal_height(),
    };
}

}  // namespace esc
