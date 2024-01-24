#ifndef ESC_DETAIL_MASK_HPP
#define ESC_DETAIL_MASK_HPP
#include <type_traits>

#include <esc/detail/is_scoped_enum.hpp>

namespace esc::detail {

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
    [[nodiscard]] auto constexpr contains(E e) const -> bool
    {
        return (flags_ & ut_cast(e)) > 0;
    }

    /// Modifies the container by adding the flag \p e.
    /** Returns a reference to the container with the new flag inserted. */
    auto constexpr insert(E e) -> Mask&
    {
        flags_ |= ut_cast(e);
        return *this;
    }

    /// Modifies the container by adding all flags from the Mask \p m.
    /** Returns a reference to the container with the new flag inserted. */
    auto constexpr insert(Mask m) -> Mask&
    {
        flags_ |= m.data();
        return *this;
    }

    /// Modifies the container by removing the flag \p e.
    /** Returns a reference to the container. */
    auto constexpr remove(E e) -> Mask&
    {
        flags_ &= ~(ut_cast(e));
        return *this;
    }

    /// Modifies the container by removing the flags contained within \p m.
    /** Returns a reference to the container. */
    auto constexpr remove(Mask m) -> Mask&
    {
        flags_ &= ~(m.data());
        return *this;
    }

    /// Returns the underlying bitmask representation of the container.
    [[nodiscard]] auto constexpr data() const -> std::underlying_type_t<E>
    {
        return flags_;
    }

   private:
    std::underlying_type_t<E> flags_;

   private:
    /// Return \p e as its underlying type.
    static auto constexpr ut_cast(E e) -> std::underlying_type_t<E>
    {
        return static_cast<std::underlying_type_t<E>>(e);
    }
};

/// Return true if data() members are equal.
template <typename E>
[[nodiscard]] auto constexpr operator==(Mask<E> a, Mask<E> b) -> bool
{
    return a.data() == b.data();
}

/// Return true if data() members are not equal.
template <typename E>
[[nodiscard]] auto constexpr operator!=(Mask<E> a, Mask<E> b) -> bool
{
    return a.data() != b.data();
}

}  // namespace esc::detail
#endif  // ESC_DETAIL_MASK_HPP
