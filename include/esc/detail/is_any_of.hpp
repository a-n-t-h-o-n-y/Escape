#ifndef ESC_DETAIL_IS_ANY_OF_HPP
#define ESC_DETAIL_IS_ANY_OF_HPP
#include <type_traits>

namespace esc::detail {

/// Type Trait defined as true if T is_same as any of Us.
template <typename T, typename... Args>
bool constexpr is_any_of = (std::is_same_v<Args, T> || ...);

}  // namespace esc::detail
#endif  // ESC_DETAIL_IS_ANY_OF_HPP
