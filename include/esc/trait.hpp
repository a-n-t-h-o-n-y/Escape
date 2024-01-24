#pragma once

#include <cstdint>
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

/// Return Trait's named string representation.
[[nodiscard]] auto to_string(Trait t) -> std::string;

/// Mask type for the Trait flag.
using Traits = detail::Mask<Trait>;

/// Used to request a Trait be removed from a Traits mask.
struct RemoveTrait {
    Trait trait;
};

/// Create a RemoveTrait object
[[nodiscard]] constexpr auto remove_trait(Trait t) -> RemoveTrait
{
    return RemoveTrait{t};
}

/// Mask creating insert operation, non-modifying, returns the new value.
[[nodiscard]] auto constexpr operator|(Trait a, Trait b) -> Traits
{
    return Traits{a}.insert(b);
}

/// Insert operation, non-modifying, returns the new value.
[[nodiscard]] auto constexpr operator|(Traits a, Trait b) -> Traits
{
    return a.insert(b);
}

/// Insert operation, non-modifying, returns the new value.
[[nodiscard]] auto constexpr operator|(Traits a, RemoveTrait b) -> Traits
{
    return a.remove(b.trait);
}

}  // namespace esc
