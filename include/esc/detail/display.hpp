#ifndef ESC_DETAIL_DISPLAY_HPP
#define ESC_DETAIL_DISPLAY_HPP
#include <string>

namespace esc::detail {

/// Translate a single non-visible character into a visible string.
/** Visible chars pass through unchanged, into a string. */
[[nodiscard]] inline auto display(char c) -> std::string
{
    switch (c) {
        case '\x9b': return "CSI";
        case '\033': return "\\033";
        case '\n': return "\\n";
        case '\t': return "\\t";
        case '\0': return "\\0";
        case '\010': return "\\010";
        default: return std::string(1, c);
    }
}

/// Translate any control characters to a visible byte string.
/** Visible chars pass through unchanged. */
[[nodiscard]] inline auto display(std::string const& sequence) -> std::string
{
    auto result = std::string{};
    for (auto c : sequence)
        result.append(display(c));
    return result;
}

}  // namespace esc::detail
#endif  // ESC_DETAIL_DISPLAY_HPP
