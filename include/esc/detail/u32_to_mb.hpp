#pragma once

#include <string>

namespace esc::detail {

/// Convert the given char32_t into a multi-byte reprensentation as a string.
/// @param c The char32_t to convert.
/// @return The multi-byte representation of the given char32_t.
/// @throws std::runtime_error if the conversion fails.
[[nodiscard]] auto u32_to_mb(char32_t c) -> std::string;

}  // namespace esc::detail