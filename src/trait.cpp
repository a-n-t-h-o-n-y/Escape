#include <esc/trait.hpp>

#include <stdexcept>
#include <string>

namespace esc {

auto to_string(Trait t) -> std::string
{
    switch (t) {
        case Trait::None: return "None"; break;
        case Trait::Standout: return "Standout"; break;
        case Trait::Bold: return "Bold"; break;
        case Trait::Dim: return "Dim"; break;
        case Trait::Italic: return "Italic"; break;
        case Trait::Underline: return "Underline"; break;
        case Trait::Blink: return "Blink"; break;
        case Trait::Inverse: return "Inverse"; break;
        case Trait::Invisible: return "Invisible"; break;
        case Trait::Crossed_out: return "Crossed_out"; break;
        case Trait::Double_underline: return "Double_underline"; break;
        default: break;
    }
    throw std::runtime_error{"to_string(Trait): Invalid Trait."};
}

}  // namespace esc
