#ifndef ESC_DETAIL_IS_CONVERTIBLE_TO_ANY_OF_HPP
#define ESC_DETAIL_IS_CONVERTIBLE_TO_ANY_OF_HPP
#include <type_traits>

namespace esc::detail {

/// Type Trait defined as true if T is_same as any of Us
template <typename T, typename... Args>
bool constexpr is_convertible_to_any_of = (std::is_convertible_v<T, Args> ||
                                           ...);

}  // namespace esc::detail
#endif  // ESC_DETAIL_IS_CONVERTIBLE_TO_ANY_OF_HPP
