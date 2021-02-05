#ifndef ESC_DETAIL_TO_BYTES_HPP
#define ESC_DETAIL_TO_BYTES_HPP
#include <array>
#include <climits>
#include <cstddef>
#include <stdexcept>
#include <utility>

#ifdef __APPLE__
#    include <cwchar>
#else
#    include <cuchar>
#endif

namespace esc::detail {

/// Convert the given char32_t into a multi-byte array of chars.
/** Depends on the currently set clocale to transform the char32_t. */
[[nodiscard]] inline auto to_bytes(char32_t c)
    -> std::pair<std::size_t, std::array<char, MB_LEN_MAX>>
{
    auto result = std::array<char, MB_LEN_MAX>{};
    auto state  = std::mbstate_t{};

#ifdef __APPLE__
    static_assert(sizeof(wchar_t) == sizeof(char32_t));
    static_assert(alignof(wchar_t) == alignof(char32_t));
    auto const count =
        std::wcrtomb(result.data(), static_cast<wchar_t>(c), &state);
#else
    auto const count = std::c32rtomb(result.data(), c, &state);
#endif
    if (count == std::size_t(-1))
        throw std::runtime_error{"to_bytes(char32_t): Error in Conversion."};
    return {count, result};
}

}  // namespace esc::detail
#endif  // ESC_DETAIL_TO_BYTES_HPP
