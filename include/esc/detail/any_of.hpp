#pragma once

#include <concepts>

namespace esc::detail {

/**
 * Concept that checks if T is the same as any of Args.
 *
 * @details This does not check reference qualifiers, std::decay is used on T.
 * @tparam T The type to check.
 * @tparam Args The types to check against.
 */
template <typename T, typename... Args>
concept AnyOf = (std::is_same_v<Args, std::decay_t<T>> || ...);

}  // namespace esc::detail