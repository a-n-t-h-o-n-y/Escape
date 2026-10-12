#pragma once

#include <cstdint>
#include <stdexcept>

namespace esc {

/// Modeled SGR attributes. Standout combines bold and inverse.
enum class Trait : std::uint16_t {
    None = 0,
    Bold = 1 << 1,
    Dim = 1 << 2,
    Italic = 1 << 3,
    Underline = 1 << 4,
    Blink = 1 << 5,
    Inverse = 1 << 6,
    Invisible = 1 << 7,
    CrossedOut = 1 << 8,
    DoubleUnderline = 1 << 9,
    Standout = (1 << 1) | (1 << 6),
};

/// Only modeled attributes can enter the mask. Standout is a combination,
/// and None is empty; there is no separate redundant Standout bit.
class Traits {
   public:
    constexpr Traits() = default;

    /// Construct a validated mask; unknown bits throw invalid_argument.
    constexpr Traits(Trait trait) : flags_{bits(trait)} {}

    /// Return the modeled bit mask.
    [[nodiscard]] constexpr auto data() const noexcept -> std::uint16_t
    { return flags_; }

    /// Test all requested bits. Trait::None tests whether this mask is empty.
    [[nodiscard]] constexpr auto contains(Trait trait) const -> bool
    {
        auto const requested = bits(trait);
        return requested == 0 ? flags_ == 0 : (flags_ & requested) == requested;
    }

    /// Enable a validated attribute and return this mask.
    constexpr auto insert(Trait trait) -> Traits&
    {
        flags_ |= bits(trait);
        return *this;
    }

    /// Enable the supplied attributes and return this mask.
    constexpr auto insert(Traits traits) -> Traits&
    {
        flags_ |= traits.flags_;
        return *this;
    }

    /// Disable a validated attribute and return this mask.
    constexpr auto remove(Trait trait) -> Traits&
    {
        flags_ &= static_cast<std::uint16_t>(~bits(trait));
        return *this;
    }

    /// Disable the supplied attributes and return this mask.
    constexpr auto remove(Traits traits) -> Traits&
    {
        flags_ &= static_cast<std::uint16_t>(~traits.flags_);
        return *this;
    }

    [[nodiscard]] constexpr auto operator==(Traits const&) const -> bool = default;

   private:
    static constexpr auto bits(Trait trait) -> std::uint16_t
    {
        auto const value = static_cast<std::uint16_t>(trait);
        constexpr std::uint16_t modeled = (1 << 10) - 2;
        if (value & ~modeled) throw std::invalid_argument{"Traits: unknown trait bits"};
        return value;
    }

    std::uint16_t flags_ = 0;
};

/// A partial style change that disables these attributes.
struct WithoutTraits {
    Traits traits;
};

/// Create an attribute-removal patch for use with styling and composition.
[[nodiscard]] constexpr auto without(Traits traits) -> WithoutTraits
{ return {traits}; }

inline constexpr Trait bold = Trait::Bold;
inline constexpr Trait dim = Trait::Dim;
inline constexpr Trait italic = Trait::Italic;
inline constexpr Trait underline = Trait::Underline;
inline constexpr Trait blink = Trait::Blink;
inline constexpr Trait inverse = Trait::Inverse;
inline constexpr Trait invisible = Trait::Invisible;
inline constexpr Trait crossed_out = Trait::CrossedOut;
inline constexpr Trait double_underline = Trait::DoubleUnderline;
inline constexpr Trait standout = Trait::Standout;

/// Combine two validated attributes into a mask.
[[nodiscard]] constexpr auto operator|(Trait a, Trait b) -> Traits
{ return Traits{a}.insert(b); }

/// Return the union of a mask and an attribute.
[[nodiscard]] constexpr auto operator|(Traits a, Trait b) -> Traits
{ return a.insert(b); }

/// Return the union of an attribute and a mask.
[[nodiscard]] constexpr auto operator|(Trait a, Traits b) -> Traits
{ return b.insert(a); }

/// Return the union of two masks.
[[nodiscard]] constexpr auto operator|(Traits a, Traits b) -> Traits
{ return a.insert(b); }

}  // namespace esc
