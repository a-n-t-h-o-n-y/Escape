#pragma once

#include <string>

namespace esc::detail {

/// Translate a single non-visible character into a visible string.
/** Visible chars pass through unchanged, into a string. */
[[nodiscard]] auto display(char c) -> std::string;

/// Translate any control characters to a visible byte string.
/** Visible chars pass through unchanged. */
[[nodiscard]] auto display(std::string const& sequence) -> std::string;

}  // namespace esc::detail