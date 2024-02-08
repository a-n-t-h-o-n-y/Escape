#include <esc/sequence.hpp>

#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>

#include <esc/brush.hpp>
#include <esc/color.hpp>
#include <esc/detail/transcode.hpp>
#include <esc/glyph.hpp>
#include <esc/trait.hpp>

namespace {

// Mutable, currently set Traits & Colors. Yes, they are global to this TU.
auto current_traits     = esc::Traits{esc::Trait::None};
auto current_background = esc::Color{esc::DefaultColor{}};
auto current_foreground = esc::Color{esc::DefaultColor{}};

/**
 * Translate a single Trait into its control sequence parameter integer.
 *
 * @details Returns empty string for Trait::None.
 * @param   t The Trait to translate.
 * @return  The control sequence parameter int as string.
 * @throws  std::runtime_error If the Trait is invalid.
 */
auto trait_to_int_sequence(esc::Trait t) -> std::string
{
    using esc::Trait;
    switch (t) {
        case Trait::Standout:
            return trait_to_int_sequence(Trait::Bold) + ';' +
                   trait_to_int_sequence(Trait::Inverse);
        case Trait::None: return "";
        case Trait::Bold: return "1";
        case Trait::Dim: return "2";
        case Trait::Italic: return "3";
        case Trait::Underline: return "4";
        case Trait::Blink: return "5";
        case Trait::Inverse: return "7";
        case Trait::Invisible: return "8";
        case Trait::Crossed_out: return "9";
        case Trait::Double_underline: return "21";
    }
    throw std::runtime_error{"traits_to_int_sequence: Invalid Trait Value"};
}

/**
 * Turn a mask of traits into a string control sequence of parameter integers.
 *
 * @param  traits The traits to translate.
 * @return The control sequence parameter integers as a string.
 * @throws std::logic_error If a Trait is invalid.
 */
auto traits_to_int_sequence(esc::Traits traits) -> std::string
{
    using esc::Trait;
    auto constexpr last_trait = 512;

    auto result = std::string{};
    for (auto i = std::underlying_type_t<Trait>{1}; i <= last_trait; i <<= 1) {
        if (auto const t = static_cast<Trait>(i); traits.contains(t)) {
            result.append(trait_to_int_sequence(t));
            result.push_back(';');
        }
    }
    if (!result.empty()) {
        result.pop_back();  // Remove last semi-colon.
    }
    return result;
}

}  // namespace

namespace esc {

auto escape(Cursor p) -> std::string
{
    return "\033[" + std::to_string(p.y + 1) + ';' + std::to_string(p.x + 1) +
           'H';
}

auto escape(BlankRow) -> std::string
{
    return "\033["
           "2K";
}

auto escape(BlankScreen) -> std::string
{
    return "\033["
           "2J";
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
               ::traits_to_int_sequence(traits) + 'm';
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

auto escape_bg(XColor c) -> std::string
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

auto escape_fg(XColor c) -> std::string
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
    return escape(bg(b.background)) + escape(fg(b.foreground)) +
           escape(b.traits);
}

}  // namespace esc
