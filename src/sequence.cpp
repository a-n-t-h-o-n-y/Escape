#include <esc/sequence.hpp>

#include <string>
#include <variant>

#include <esc/detail/traits_to_int_sequence.hpp>
#include <esc/detail/transcode.hpp>

namespace {

// Modifiable currently set Traits & Colors, yes, they are global to this TU.
auto current_traits     = esc::Traits{esc::Trait::None};
auto current_background = esc::Color{esc::DefaultColor{}};
auto current_foreground = esc::Color{esc::DefaultColor{}};

}  // namespace

namespace esc {

auto escape(Cursor_position p) -> std::string
{
    return "\033[" + std::to_string(p.at.y + 1) + ';' +
           std::to_string(p.at.x + 1) + 'H';
}

auto escape(Blank_row) -> std::string
{
    return "\033["
           "2K";
}

auto escape(Blank_screen) -> std::string
{
    return "\033["
           "?2J";
}

auto escape(Traits traits) -> std::string
{
    ::current_traits = traits;
    if (traits.data() == 0) {  // Trait::None
        // Clear Traits
        return "\033["
               "22;23;24;25;27;28;29m";
    }
    else {
        return "\033["
               "22;23;24;25;27;28;29;" +
               detail::traits_to_int_sequence(traits) + 'm';
        // TODO maybe anon namespace instead of detail? if only used here.
    }
}

auto escape(Trait trait) -> std::string { return escape(Traits{trait}); }

auto clear_traits() -> std::string
{
    ::current_traits = Traits{};
    return "\033["
           "22;23;24;25;27;28;29m";
}

auto traits() -> Traits { return ::current_traits; }

auto escape(ColorBG c) -> std::string
{
    return std::visit([](auto c) { return escape_bg(c); }, c.value);
}

auto escape_bg(ColorIndex c) -> std::string
{
    ::current_background = c;
    return "\033["
           "48;5;" +
           std::to_string(c.value) + 'm';
}

auto escape_bg(TrueColor c) -> std::string
{
    ::current_background = c;
    return "\033["
           "48;2;" +
           std::to_string(c.red) + ';' + std::to_string(c.green) + ';' +
           std::to_string(c.blue) + 'm';
}

auto escape_bg(DefaultColor c) -> std::string
{
    ::current_background = c;
    return "\033["
           "49m";
}

auto background_color() -> Color { return ::current_background; }

auto escape(ColorFG c) -> std::string
{
    return std::visit([](auto c) { return escape_fg(c); }, c.value);
}

auto escape_fg(ColorIndex c) -> std::string
{
    ::current_foreground = c;
    return "\033["
           "38;5;" +
           std::to_string(c.value) + 'm';
}

auto escape_fg(TrueColor c) -> std::string
{
    ::current_foreground = c;
    return "\033["
           "38;2;" +
           std::to_string(c.red) + ';' + std::to_string(c.green) + ';' +
           std::to_string(c.blue) + 'm';
}

auto escape_fg(DefaultColor c) -> std::string
{
    ::current_foreground = c;
    return "\033["
           "39m";
}

auto foreground_color() -> Color { return ::current_foreground; }

auto escape(Brush b) -> std::string
{
    return escape(background(b.background)) + escape(foreground(b.foreground)) +
           escape(b.traits);
}

auto escape(Glyph const& g) -> std::string
{
    return escape(g.brush) + detail::u32_to_u8(g.symbol);
}

}  // namespace esc
