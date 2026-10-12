#pragma once

#include <cstdint>
#include <stdexcept>

namespace esc {

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

// Only modeled attributes can enter the mask. Standout is a combination,
// and None is empty; there is no separate redundant Standout bit.
class Traits {
   public:
    constexpr Traits() = default;

    constexpr Traits(Trait trait) : flags_{bits(trait)} {}

    [[nodiscard]] constexpr auto data() const noexcept -> std::uint16_t
    { return flags_; }

    [[nodiscard]] constexpr auto contains(Trait trait) const -> bool
    {
        auto const requested = bits(trait);
        return requested == 0 ? flags_ == 0 : (flags_ & requested) == requested;
    }

    constexpr auto insert(Trait trait) -> Traits&
    {
        flags_ |= bits(trait);
        return *this;
    }

    constexpr auto insert(Traits traits) -> Traits&
    {
        flags_ |= traits.flags_;
        return *this;
    }

    constexpr auto remove(Trait trait) -> Traits&
    {
        flags_ &= static_cast<std::uint16_t>(~bits(trait));
        return *this;
    }

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

struct WithoutTraits {
    Traits traits;
};

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

[[nodiscard]] constexpr auto operator|(Trait a, Trait b) -> Traits
{ return Traits{a}.insert(b); }

[[nodiscard]] constexpr auto operator|(Traits a, Trait b) -> Traits
{ return a.insert(b); }

[[nodiscard]] constexpr auto operator|(Trait a, Traits b) -> Traits
{ return b.insert(a); }

[[nodiscard]] constexpr auto operator|(Traits a, Traits b) -> Traits
{ return a.insert(b); }

}  // namespace esc
