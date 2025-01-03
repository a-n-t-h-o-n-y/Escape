#pragma once

#include <cstdint>
#include <string>

#include <esc/detail/mask.hpp>

namespace esc {

/**
 * Visual Traits that can be applied to terminal text with set_traits(...).
 */
enum class Trait : std::uint16_t {
    None = 0,
    Standout = 1 << 0,
    Bold = 1 << 1,
    Dim = 1 << 2,
    Italic = 1 << 3,
    Underline = 1 << 4,
    Blink = 1 << 5,
    Inverse = 1 << 6,
    Invisible = 1 << 7,
    Crossed_out = 1 << 8,
    Double_underline = 1 << 9,
};

/**
 * Return Trait \p t as a descriptive string.
 * @param t The Trait to convert to a string.
 * @return The string representation of the Trait.
 * @throws std::runtime_error if t is not a valid Trait.
 */
[[nodiscard]] auto to_string(Trait t) -> std::string;

/**
 * Mask type for the Trait flag to represent multiple Trait objects.
 */
using Traits = detail::Mask<Trait>;

/**
 * This is used to request a Trait be removed from a Traits mask with pipe ops.
 */
struct RemoveTrait {
    Trait trait;
};

/**
 * Create a RemoveTrait object to remove a Trait from a Traits mask.
 * @param t The Trait to remove from a Traits mask.
 * @return A RemoveTrait object to remove the Trait from a Traits mask.
 */
[[nodiscard]] constexpr auto remove_trait(Trait t) -> RemoveTrait
{
    return RemoveTrait{t};
}

/**
 * Combine two Trait objects into a Traits mask.
 * @param a The first Trait to combine.
 * @param b The second Trait to combine.
 * @return A Traits mask with both Trait objects combined.
 */
[[nodiscard]] auto constexpr operator|(Trait a, Trait b) -> Traits
{
    return Traits{a}.insert(b);
}

/**
 * Combine a Traits mask and a Trait object into a new Traits mask.
 * @param a The Traits mask to combine.
 * @param b The Trait to combine.
 * @return A new Traits mask with the Trait object combined.
 * @note This is a non-modifying operation.
 */
[[nodiscard]] auto constexpr operator|(Traits a, Trait b) -> Traits
{
    return a.insert(b);
}

/**
 * Combine a Trait object and a Traits mask into a new Traits mask.
 * @param a The Trait to combine.
 * @param b The Traits mask to combine.
 * @return A new Traits mask with the Trait object combined.
 * @note This is a non-modifying operation.
 */
[[nodiscard]] auto constexpr operator|(Trait a, Traits b) -> Traits
{
    return b.insert(a);
}

/**
 * Remove a Trait from a Traits mask.
 * @param a The Traits mask to remove the Trait from.
 * @param b The Trait to remove from the Traits mask.
 * @return A new Traits mask with the Trait removed.
 * @note This is a non-modifying operation.
 */
[[nodiscard]] auto constexpr operator|(Traits a, RemoveTrait b) -> Traits
{
    return a.remove(b.trait);
}

}  // namespace esc
