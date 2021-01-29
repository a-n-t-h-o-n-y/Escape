#include <esc/terminfo.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

/// Very basic info about a particular terminal emulator.
/** Currently just color_palette_size. Was going to contain supported mouse
 *  modes, but terminfo doesn't seem to have that. */
struct Terminfo {
    std::string_view TERM_name;
    std::uint16_t color_palette_size;
};

/// Small database of term names and color counts.
/** Generated with tools/terminfo.sh */
inline auto const terminfo_db = std::array<Terminfo, 140>{
    Terminfo{"xterm-256color", 256},
    Terminfo{"tmux-256color", 256},
    Terminfo{"tmux", 8},
    Terminfo{"cygwin", 8},
    Terminfo{"putty", 8},
    Terminfo{"putty-256color", 256},
    Terminfo{"linux", 8},
    Terminfo{"vte-256color", 256},
    Terminfo{"konsole", 8},
    Terminfo{"konsole-256color", 256},
    Terminfo{"kitty", 256},
    Terminfo{"xterm-kitty", 256},
    Terminfo{"Apple_Terminal", 256},
    Terminfo{"rxvt-unicode", 88},
    Terminfo{"rxvt-unicode-256color", 256},
    Terminfo{"rxvt-256color", 256},
    Terminfo{"teraterm2.3", 8},
    Terminfo{"teraterm", 8},
    Terminfo{"xterm-xf86-v40", 8},
    Terminfo{"xterm-noapp", 8},
    Terminfo{"xterm-24", 8},
    Terminfo{"xterm-88color", 88},
    Terminfo{"xterm-r5", 8},
    Terminfo{"xterm-xf86-v33", 8},
    Terminfo{"xterm-hp", 8},
    Terminfo{"xterm-x11hilite", 8},
    Terminfo{"xterm-xf86-v44", 8},
    Terminfo{"xterm-pcolor", 8},
    Terminfo{"xterm-xfree86", 8},
    Terminfo{"xterm-xf86-v333", 8},
    Terminfo{"xterm-direct2", 256},
    Terminfo{"xterm-xf86-v43", 8},
    Terminfo{"xterm-xi", 8},
    Terminfo{"xterm-sun", 8},
    Terminfo{"xterm-1005", 8},
    Terminfo{"xterm-utf8", 8},
    Terminfo{"xterm-1006", 8},
    Terminfo{"xterm-x10mouse", 8},
    Terminfo{"xterm-1002", 8},
    Terminfo{"xterm-8bit", 8},
    Terminfo{"xterm-vt220", 8},
    Terminfo{"xterm", 8},
    Terminfo{"xterm-bold", 8},
    Terminfo{"xterm-direct", 256},
    Terminfo{"xterm-16color", 16},
    Terminfo{"xterm-color", 8},
    Terminfo{"xterm-r6", 8},
    Terminfo{"xterms", 8},
    Terminfo{"xterm-old", 8},
    Terminfo{"xterm-x11mouse", 8},
    Terminfo{"xfce", 8},
    Terminfo{"xterm-new", 8},
    Terminfo{"xterm-sco", 8},
    Terminfo{"xterm-mono", 8},
    Terminfo{"xterm-nic", 8},
    Terminfo{"xterm-1003", 8},
    Terminfo{"xterm-vt52", 8},
    Terminfo{"xterm-basic", 8},
    Terminfo{"xterm-xf86-v32", 8},
    Terminfo{"screen.konsole-256color", 256},
    Terminfo{"screen-256color", 256},
    Terminfo{"screen.Eterm", 8},
    Terminfo{"screen.linux", 8},
    Terminfo{"screen.putty", 8},
    Terminfo{"screen.vte-256color", 256},
    Terminfo{"screen.xterm-r6", 8},
    Terminfo{"st-16color", 16},
    Terminfo{"sun1", 8},
    Terminfo{"sun2", 8},
    Terminfo{"stterm-256color", 256},
    Terminfo{"screen.mlterm-256color", 256},
    Terminfo{"screen.vte", 8},
    Terminfo{"screen.gnome", 8},
    Terminfo{"screen.putty-256color", 256},
    Terminfo{"screen.xterm-new", 8},
    Terminfo{"stterm-16color", 16},
    Terminfo{"screen-16color", 16},
    Terminfo{"screen.xterm-256color", 256},
    Terminfo{"screen.konsole", 8},
    Terminfo{"st", 8},
    Terminfo{"screen", 8},
    Terminfo{"screen.rxvt", 8},
    Terminfo{"screen.teraterm", 8},
    Terminfo{"screen.xterm-xfree86", 8},
    Terminfo{"stterm", 8},
    Terminfo{"screen.mlterm", 8},
    Terminfo{"st-256color", 256},
    Terminfo{"screen.mrxvt", 8},
    Terminfo{"sun", 8},
    Terminfo{"mach", 8},
    Terminfo{"mach-bold", 8},
    Terminfo{"mlterm", 8},
    Terminfo{"mach-color", 8},
    Terminfo{"mach-gnu", 8},
    Terminfo{"mach-gnu-color", 8},
    Terminfo{"mrxvt", 8},
    Terminfo{"gnome-256color", 256},
    Terminfo{"gnome", 8},
    Terminfo{"Eterm-88color", 88},
    Terminfo{"Eterm-256color", 256},
    Terminfo{"Eterm-color", 8},
    Terminfo{"Eterm", 8},
    Terminfo{"dumb", 8},
    Terminfo{"rxvt-cygwin", 8},
    Terminfo{"rxvt-color", 8},
    Terminfo{"rxvt", 8},
    Terminfo{"rxvt-basic", 8},
    Terminfo{"rxvt-xpm", 8},
    Terminfo{"rxvt-cygwin-native", 8},
    Terminfo{"rxvt-16color", 16},
    Terminfo{"rxvt-88color", 88},
    Terminfo{"nsterm", 256},
    Terminfo{"nsterm-256color", 256},
    Terminfo{"nxterm", 8},
    Terminfo{"aterm", 8},
    Terminfo{"ansis", 8},
    Terminfo{"ansi", 8},
    Terminfo{"alacritty", 256},
    Terminfo{"ansi80x25", 8},
    Terminfo{"hurd", 8},
    Terminfo{"kon2", 8},
    Terminfo{"kon", 8},
    Terminfo{"vt100-nav", 8},
    Terminfo{"vs100", 8},
    Terminfo{"vt52", 8},
    Terminfo{"vt200", 8},
    Terminfo{"vte", 8},
    Terminfo{"vwmterm", 8},
    Terminfo{"vt100-am", 8},
    Terminfo{"vt102", 8},
    Terminfo{"vt100", 8},
    Terminfo{"vt220", 8},
    Terminfo{"jfbterm", 8},
    Terminfo{"eterm-color", 8},
    Terminfo{"eterm", 8},
    Terminfo{"wsvt25m", 8},
    Terminfo{"wsvt25", 8},
    Terminfo{"pcansi", 8},
    Terminfo{"bterm", 8},
    Terminfo{"cons25", 8},
};

[[nodiscard]] auto get_terminfo(std::string_view term_name) -> Terminfo
{
    auto at = std::find_if(
        std::cbegin(terminfo_db), std::cend(terminfo_db),
        [&term_name](auto const& ti) { return ti.TERM_name == term_name; });
    if (at == std::cend(terminfo_db))
        throw std::runtime_error{"get_terminfo(): term_name not found."};
    return *at;
}

auto const TERM = std::string{std::getenv("TERM")};

auto const COLORTERM = std::string{std::getenv("COLORTERM")};

auto const terminfo = get_terminfo(TERM);

}  // namespace

namespace esc {

auto get_TERM() -> std::string_view { return TERM; }

auto color_palette_size() -> std::uint16_t
{
    return terminfo.color_palette_size;
}

auto has_true_color() -> bool
{
    return COLORTERM == "truecolor" || COLORTERM == "24bit";
}

}  // namespace esc
