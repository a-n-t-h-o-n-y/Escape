#ifndef ESC_TRAIT_HPP
#define ESC_TRAIT_HPP
#include <cstdint>
#include <stdexcept>
#include <string>

#include <esc/detail/mask.hpp>

namespace esc {

/// Visual Traits that can be applied to terminal text with set_traits(...).
enum class Trait : std::uint16_t {
    None             = 0,
    Standout         = 1,
    Bold             = 2,
    Dim              = 4,
    Italic           = 8,
    Underline        = 16,
    Blink            = 32,
    Inverse          = 64,
    Invisible        = 128,
    Crossed_out      = 256,
    Double_underline = 512,
};

[[nodiscard]] inline auto to_string(Trait t) -> std::string
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

/// Mask type for the Trait flag.
using Traits = detail::Mask<Trait>;

/// Mask creating insert operation, non-modifying, returns the new value.
[[nodiscard]] constexpr auto operator|(Trait a, Trait b) -> Traits
{
    return Traits{a}.insert(b);
}

/// Insert operation, non-modifying, returns the new value.
[[nodiscard]] constexpr auto operator|(Traits a, Trait b) -> Traits
{
    return a.insert(b);
}

/// Insert assignment operation, returns a reference to the modified \p a.
constexpr auto operator|=(Traits& a, Trait b) -> Traits&
{
    a = a.insert(b);
    return a;
}

/// Merge operation, non-modifying, returns the new value.
[[nodiscard]] constexpr auto operator|(Traits a, Traits b) -> Traits
{
    return static_cast<Traits>(static_cast<Trait>(a.data() | b.data()));
}

/// Merge assignment operation, returns reference to the modified \p a.
constexpr auto operator|=(Traits& a, Traits b) -> Traits&
{
    a = (a | b);
    return a;
}

/// Return true if \p a has exactly the same set of Traits as \p b.
[[nodiscard]] constexpr auto operator==(Traits a, Traits b) -> bool
{
    return a.data() == b.data();
}

/// Return true if \p a doesn't have exactly the same set of Traits as \p b.
[[nodiscard]] constexpr auto operator!=(Traits a, Traits b) -> bool
{
    return a.data() != b.data();
}

}  // namespace esc
#endif  // ESC_TRAIT_HPP
