#pragma once

#include <type_traits>

namespace esc::detail {

template <typename T>
concept ScopedEnum =
    std::is_enum_v<T> && !std::is_convertible_v<T, std::underlying_type_t<T>>;

/**
 * Container for Scope Enum Flag Types.
 *
 * @details E must be a scoped enum and it is assumed each value in the enum is
 *          assigned a power of two.
 * @tparam E The scoped enum type to use as the flag type.
 */
template <ScopedEnum E>
class Mask {
   public:
    /**
     * Construct an empty mask, zero value.
     */
    constexpr Mask() : flags_{0} {}

    /**
     * Construct a mask from a single flag.
     */
    constexpr Mask(E e) : flags_{ut_cast(e)} {}

   public:
    /**
     * Check if the mask contains the flag \p e.
     *
     * @param e The flag to check for.
     * @return True if the mask contains the flag \p e.
     */
    [[nodiscard]] constexpr auto contains(E e) const -> bool
    {
        return (flags_ & ut_cast(e)) > 0;
    }

    /**
     * Modifies the container by adding the flag \p e.
     *
     * @param e The flag to insert.
     * @return A reference to the container with the new flag inserted.
     */
    constexpr auto insert(E e) -> Mask&
    {
        flags_ |= ut_cast(e);
        return *this;
    }

    /**
     * Modifies the container by adding all flags from the Mask \p m.
     *
     * @param m The mask to insert.
     * @return A reference to the container with the new flags inserted.
     */
    constexpr auto insert(Mask m) -> Mask&
    {
        flags_ |= m.data();
        return *this;
    }

    /**
     * Modifies the container by removing the flag \p e.
     *
     * @param e The flag to remove.
     * @return A reference to the container with the flag removed.
     */
    constexpr auto remove(E e) -> Mask&
    {
        flags_ &= static_cast<std::underlying_type_t<E>>(~(ut_cast(e)));
        return *this;
    }

    /**
     * Modifies the container by removing the flags contained within \p m.
     *
     * @param m The mask to remove.
     * @return A reference to the container with the flags removed.
     */
    constexpr auto remove(Mask m) -> Mask&
    {
        flags_ &= ~(m.data());
        return *this;
    }

    /**
     * Returns the underlying bitmask representation of the container.
     *
     * @return std::underlying_type_t<E> The underlying bitmask representation.
     */
    [[nodiscard]] constexpr auto data() const -> std::underlying_type_t<E>
    {
        return flags_;
    }

   private:
    std::underlying_type_t<E> flags_;

   private:
    /**
     * Return \p e as its underlying type.
     *
     * @param e The enum value to cast.
     * @return std::underlying_type_t<E> The underlying type of \p e.
     */
    static constexpr auto ut_cast(E e) -> std::underlying_type_t<E>
    {
        return static_cast<std::underlying_type_t<E>>(e);
    }
};

/**
 * Check if two Mask objects are equal.
 *
 * @tparam E The scoped enum type of the Mask objects.
 * @param a The first Mask to compare.
 * @param b The second Mask to compare.
 * @return True if the data() members are equal.
 */
template <typename E>
[[nodiscard]] constexpr auto operator==(Mask<E> a, Mask<E> b) -> bool
{
    return a.data() == b.data();
}

/**
 * Check if two Mask objects are not equal.
 *
 * @tparam E The scoped enum type of the Mask objects.
 * @param a The first Mask to compare.
 * @param b The second Mask to compare.
 * @return True if the data() members are not equal.
 */
template <typename E>
[[nodiscard]] constexpr auto operator!=(Mask<E> a, Mask<E> b) -> bool
{
    return a.data() != b.data();
}

}  // namespace esc::detail