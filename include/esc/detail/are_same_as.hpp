#ifndef ESC_DETAIL_ARE_SAME_AS_HPP
#define ESC_DETAIL_ARE_SAME_AS_HPP
#include <type_traits>

namespace esc::detail {

// TODO Remove if not going to use.

/// True if all Args... types are the same type as T.
template <typename T, typename... Args>
inline bool constexpr are_same_as = (std::is_same_v<T, Args> && ...);

}  // namespace esc::detail
#endif  // ESC_DETAIL_ARE_SAME_AS_HPP
