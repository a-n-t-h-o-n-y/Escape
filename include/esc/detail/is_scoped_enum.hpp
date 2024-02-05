#pragma once

#include <type_traits>

namespace esc::detail {

// is enum and can not be converted to int.
template <typename E>
constexpr bool is_scoped_enum =
    std::is_enum_v<E> && !std::is_convertible_v<E, int>;

}  // namespace esc::detail