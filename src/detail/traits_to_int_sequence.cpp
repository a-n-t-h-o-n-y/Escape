#include <esc/detail/traits_to_int_sequence.hpp>

#include <stdexcept>
#include <string>
#include <type_traits>

#include <esc/trait.hpp>

namespace esc::detail {

auto trait_to_int_sequence(Trait t) -> std::string
{
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
    throw std::logic_error{"traits_to_int_sequence: Invalid Trait Value"};
}

auto traits_to_int_sequence(Traits traits) -> std::string
{
    auto constexpr last_trait = 512;

    auto result = std::string{};
    for (auto i = std::underlying_type_t<Trait>{1}; i <= last_trait; i <<= 1) {
        if (auto const t = static_cast<Trait>(i); traits.contains(t)) {
            result.append(trait_to_int_sequence(t));
            result.push_back(';');
        }
    }
    if (!result.empty())
        result.pop_back();  // Remove last semi-colon.
    return result;
}

}  // namespace esc::detail
