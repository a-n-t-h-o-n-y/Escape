#include <esc/terminal.hpp>

#include <clocale>
#include <stdexcept>
#include <string>
#include <utility>

#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <esc/io.hpp>
#include <esc/mouse.hpp>

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
    auto const canon_flag = [ib] {
        if (ib == esc::Input_buffer::Canonical)
            return ICANON;
        else
            return ~(ICANON);
    }();

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
    if (sb == Screen_buffer::Normal) {
        write(
            "\033["
            "?1049l");
    }
    else {
        write(
            "\033["
            "?1049h");
    }
}

void set(Cursor c)
{
    if (c == Cursor::Show) {
        write(
            "\033["
            "?25h");
    }
    else {
        write(
            "\033["
            "?25l");
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
                         Signals signals)
{
    auto constexpr stdout_buf_size = 4'096;

    original_clocale = std::setlocale(LC_ALL, nullptr);
    original_termios = current_termios();

    if (std::setlocale(LC_ALL, "en_US.UTF-8") == nullptr)
        throw std::runtime_error{"initialize_terminal: setlocale() failed."};
    std::setvbuf(stdout, nullptr, _IOFBF, stdout_buf_size);
    detail::register_SIGWINCH();

    fix_ctrl_m();

    set(echo, input_buffer, signals, screen_buffer, mouse_mode, cursor);
    flush();
}

void uninitialize_terminal()
{
    set(Screen_buffer::Normal, Mouse_mode::Off, Cursor::Show);
    flush();
    if (std::setlocale(LC_ALL, original_clocale.c_str()) == nullptr)
        throw std::runtime_error{"uninitialize_terminal: set_locale() failed."};
    ::tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios);
}

auto terminal_width() -> std::size_t
{
    auto w            = ::winsize{};
    auto const result = ::ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    if (result == -1) {
        throw std::runtime_error{
            "terminal.cpp terminal_width(): Can't Read Window Size."};
    }
    return w.ws_col;
}

auto terminal_height() -> std::size_t
{
    auto w            = ::winsize{};
    auto const result = ::ioctl(STDIN_FILENO, TIOCGWINSZ, &w);
    if (result == -1) {
        throw std::runtime_error{
            "terminal.cpp terminal_height(): Can't Read Window Size."};
    }
    return w.ws_row;
}

}  // namespace esc
