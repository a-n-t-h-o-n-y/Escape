#include <clocale>
#include <esc/terminal.hpp>

#include <string>
#include <utility>

#include <termios.h>
#include <unistd.h>

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

void set_echo(Echo e)
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

void set_mode(Mode m)
{
    auto const canon_flag = [m] {
        if (m == esc::Mode::Canonical)
            return ICANON;
        else
            return ~(ICANON);
    }();

    auto settings = current_termios();
    settings.c_lflag &= canon_flag;
    ::tcsetattr(STDIN_FILENO, TCSAFLUSH, &settings);
}

void set_signals(Signals s)
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

void initialize_terminal(Echo e, Mode m, Signals s)
{
    original_clocale = std::setlocale(LC_ALL, nullptr);
    original_termios = current_termios();

    std::setlocale(LC_ALL, "en_US.UTF-8");
    std::setvbuf(stdout, nullptr, _IOFBF, 4'096);

    set_echo(e);
    set_mode(m);
    set_signals(s);

    fix_ctrl_m();
}

void uninitialize_terminal()
{
    std::setlocale(LC_ALL, original_clocale.c_str());
    ::tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios);
}

}  // namespace esc
