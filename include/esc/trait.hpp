#ifndef ESC_TRAIT_HPP
#define ESC_TRAIT_HPP
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

/// Insert assignment operation, returns a reference to the modified \p a.
auto constexpr operator|=(Traits& a, Trait b) -> Traits&
{
    a = a.insert(b);
    return a;
}

/// Merge operation, non-modifying, returns the new value.
[[nodiscard]] auto constexpr operator|(Traits a, Traits b) -> Traits
{
    return static_cast<Traits>(static_cast<Trait>(a.data() | b.data()));
}

/// Merge assignment operation, returns reference to the modified \p a.
auto constexpr operator|=(Traits& a, Traits b) -> Traits&
{
    a = (a | b);
    return a;
}

/// Return true if \p a has exactly the same set of Traits as \p b.
[[nodiscard]] auto constexpr operator==(Traits a, Traits b) -> bool
{
    return a.data() == b.data();
}

/// Return true if \p a doesn't have exactly the same set of Traits as \p b.
[[nodiscard]] auto constexpr operator!=(Traits a, Traits b) -> bool
{
    return a.data() != b.data();
}

}  // namespace esc
#endif  // ESC_TRAIT_HPP
