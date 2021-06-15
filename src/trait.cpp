#include <esc/trait.hpp>

#include <stdexcept>
#include <string>

namespace esc {

auto to_string(Trait t) -> std::string
{
    switch (t) {
        case Trait::None: return "None";
        case Trait::Standout: return "Standout";
        case Trait::Bold: return "Bold";
        case Trait::Dim: return "Dim";
        case Trait::Italic: return "Italic";
        case Trait::Underline: return "Underline";
        case Trait::Blink: return "Blink";
        case Trait::Inverse: return "Inverse";
        case Trait::Invisible: return "Invisible";
        case Trait::Crossed_out: return "Crossed_out";
        case Trait::Double_underline: return "Double_underline";
    }
    throw std::runtime_error{"to_string(Trait): Invalid Trait."};
}

}  // namespace esc
