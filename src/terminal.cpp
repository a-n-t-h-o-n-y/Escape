#include <esc/terminal.hpp>

#include <clocale>
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
#include <esc/io.hpp>
#include <esc/mouse.hpp>

#include "detail/tty_file.hpp"

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
auto original_clocale = std::string{};

}  // namespace

namespace esc {

void set(Echo e)
{
    auto const echo_flag = [e] {
        if (e == esc::Echo::On)
            return ECHO;
        else
            return ~(ECHO);
    }();

    auto settings = current_termios();
    settings.c_lflag &= echo_flag;
    ::tcsetattr(STDIN_FILENO, TCSAFLUSH, &settings);
}

void set(Input_buffer ib)
{
    auto const canon_flag =
        (ib == esc::Input_buffer::Canonical) ? ICANON : ~(ICANON);

    auto settings = current_termios();
    settings.c_lflag &= canon_flag;
    if (ib == esc::Input_buffer::Immediate)
        settings.c_cc[VMIN] = 1;  // Min. number of bytes before sending.
    ::tcsetattr(STDIN_FILENO, TCSAFLUSH, &settings);
}

void set(Signals s)
{
    // ctrl-c, ctrl-z, ctrl-s, ctrl-q, ctrl-v all send their ctrl byte value
    // rather than changing the terminal's behaviour.
    auto const [lflags, iflags] = [s] {
        auto const lflags = ISIG | IEXTEN;
        auto const iflags = IXON;
        if (s == esc::Signals::On)
            return std::pair{lflags, iflags};
        else
            return std::pair{~(lflags), ~(iflags)};
    }();

    auto settings = current_termios();
    settings.c_lflag &= lflags;
    settings.c_iflag &= iflags;
    ::tcsetattr(STDIN_FILENO, TCSAFLUSH, &settings);
}

void set(Screen_buffer sb)
{
    switch (sb) {
        case Screen_buffer::Normal:
            write(
                "\033["
                "?1049l");
            break;
        case Screen_buffer::Alternate:
            write(
                "\033["
                "?1049h");
            break;
    }
}

void set(Key_mode km)
{
    switch (km) {
        case Key_mode::Normal: break;
        case Key_mode::Alternate:
            detail::tty_file_descriptor = detail::open_console_file();
            break;
    }
}

void set(Cursor c)
{
    switch (c) {
        case Cursor::Show:
            write(
                "\033["
                "?25h");
            break;
        case Cursor::Hide:
            write(
                "\033["
                "?25l");
            break;
    }
}

void set(Mouse_mode mm)
{
    auto const ext_mode = detail::is_urxvt(TERM_var()) ? "1015" : "1006";

    auto result = std::string{};
    switch (mm) {
        case Mouse_mode::Off:
            result.append("\033[?1000;1002;1003;").append(ext_mode).append("l");
            break;
        case Mouse_mode::Basic:
            result.append("\033[?1002;1003l");  // Off
            result.append("\033[?1000;").append(ext_mode).append("h");
            break;
        case Mouse_mode::Drag:
            result.append("\033[?1000;1003l");  // Off
            result.append("\033[?1002;").append(ext_mode).append("h");
            break;
        case Mouse_mode::Move:
            result.append("\033[?1000;1002l");  // Off
            result.append("\033[?1003;").append(ext_mode).append("h");
            break;
    }
    write(result);
}

void initialize_terminal(Screen_buffer screen_buffer,
                         Mouse_mode mouse_mode,
                         Cursor cursor,
                         Echo echo,
                         Input_buffer input_buffer,
                         Signals signals,
                         Key_mode key_mode)
{
    auto constexpr stdout_buf_size = 4'096;
    // TODO record current settings before calling set, this is ioctl things and
    // tcsetaddr things and locale. termios? probably from tcsetaddr

    original_clocale = std::setlocale(LC_ALL, nullptr);
    original_termios = current_termios();

    if (std::setlocale(LC_ALL, "en_US.UTF-8") == nullptr)
        throw std::runtime_error{"initialize_terminal(): setlocale() failed."};
    std::setvbuf(stdout, nullptr, _IOFBF, stdout_buf_size);
    detail::register_SIGWINCH();

    fix_ctrl_m();
    write(turn_off_auto_wrap());

    try {
        set(echo, input_buffer, signals, screen_buffer, mouse_mode, cursor,
            key_mode);
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
    initialize_terminal(Screen_buffer::Normal, Mouse_mode::Off, Cursor::Show,
                        Echo::On, Input_buffer::Canonical, Signals::On,
                        Key_mode::Normal);
}

void initialize_interactive_terminal(Mouse_mode mouse_mode, Signals signals)
{
    initialize_terminal(Screen_buffer::Alternate, mouse_mode, Cursor::Hide,
                        Echo::Off, Input_buffer::Immediate, signals,
                        Key_mode::Alternate);
}

void uninitialize_terminal()
{
    // TODO take settings parameter and use that to reset the terminal to
    // settings before.
    write(turn_on_auto_wrap());
    set(Screen_buffer::Normal, Mouse_mode::Off, Cursor::Show);
    flush();
    if (std::setlocale(LC_ALL, original_clocale.c_str()) == nullptr)
        throw std::runtime_error{"uninitialize_terminal: set_locale() failed."};
    ::tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios);
}

auto terminal_width() -> int
{
    auto w            = ::winsize{};
    auto const result = ::ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    if (result == -1) {
        throw std::runtime_error{
            "terminal.cpp terminal_width(): Can't Read Window Size."};
    }
    return w.ws_col;
}

auto terminal_height() -> int
{
    auto w            = ::winsize{};
    auto const result = ::ioctl(STDIN_FILENO, TIOCGWINSZ, &w);
    if (result == -1) {
        throw std::runtime_error{
            "terminal.cpp terminal_height(): Can't Read Window Size."};
    }
    return w.ws_row;
}

auto terminal_area() -> Area { return {terminal_width(), terminal_height()}; }

}  // namespace esc
