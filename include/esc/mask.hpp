#ifndef ESC_MASK_HPP
#define ESC_MASK_HPP
#include <type_traits>

#include <esc/detail/is_scoped_enum.hpp>

namespace esc {

/// Container for Scope Enum Flag Types.
/** E must be a scoped enum and it is assumed each value in the enum is assigned
 *  a power of two. */
template <typename E,
          typename = std::enable_if_t<detail::is_scoped_enum<E>, void>>
class Mask {
   public:
    /// Construct a mask from a single flag.
    constexpr Mask() : flags_{0} {}

    /// Construct a mask from a single flag.
    constexpr Mask(E e) : flags_{ut_cast(e)} {}

   public:
    /// Return true if \p e is contained within the mask.
    [[nodiscard]] constexpr auto contains(E e) const -> bool
    {
        return static_cast<bool>(flags_ & ut_cast(e));
    }

    /// Modifies the container by adding the flag \p e.
    /** Returns a copy of the container with the new flag inserted. */
    constexpr auto insert(E e) -> Mask<E>
    {
        flags_ |= ut_cast(e);
        return Mask<E>{static_cast<E>(flags_)};
    }

    /// Modifies the container by removing the flag \p e.
    /** Returns a copy of the container with the given flag removed . */
    constexpr auto remove(E e) -> Mask<E>
    {
        flags_ &= ~(ut_cast(e));
        return Mask<E>{static_cast<E>(flags_)};
    }

    /// Returns the underlying bitmask representation of the container.
    [[nodiscard]] constexpr auto data() const -> std::underlying_type_t<E>
    {
        return flags_;
    }

   private:
    std::underlying_type_t<E> flags_;

   private:
    /// Return \p e as its underlying type.
    static auto ut_cast(E e) -> std::underlying_type_t<E>
    {
        return static_cast<std::underlying_type_t<E>>(e);
    }
};

/// Mask creating insert operation, non-modifying, returns the new value.
template <typename E,
          typename = std::enable_if_t<detail::is_scoped_enum<E>, void>>
[[nodiscard]] inline auto operator|(E a, E b) -> Mask<E>
{
    return Mask<E>{a}.insert(b);
}

/// Insert operation, non-modifying, returns the new value.
template <typename E,
          typename = std::enable_if_t<detail::is_scoped_enum<E>, void>>
[[nodiscard]] inline auto operator|(Mask<E> a, E b) -> Mask<E>
{
    return a.insert(b);
}

}  // namespace esc
#endif  // ESC_MASK_HPP
