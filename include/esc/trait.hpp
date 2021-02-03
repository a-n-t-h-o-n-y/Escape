#ifndef ESC_TRAIT_HPP
#define ESC_TRAIT_HPP
#include <cstdint>

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

}  // namespace esc
#endif  // ESC_TRAIT_HPP
