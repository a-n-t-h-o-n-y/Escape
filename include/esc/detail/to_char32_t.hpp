#ifndef ESC_DETAIL_TO_CHAR32_T_HPP
#define ESC_DETAIL_TO_CHAR32_T_HPP
#include <array>
#include <climits>
#include <cstddef>
#include <stdexcept>

#ifdef __APPLE__
#    include <cwchar>
#else
#    include <cuchar>
#endif

namespace esc::detail {

/// Convert the given multi-byte array character into a char32_t.
/** Depends on the currently set clocale to transform the bytes. */
[[nodiscard]] inline auto to_char32_t(std::array<char, MB_LEN_MAX> bytes)
    -> char32_t
{
    auto result   = char32_t{};
    auto mb_state = std::mbstate_t{};
#ifdef __APPLE__
    static_assert(sizeof(wchar_t) == sizeof(char32_t));
    static_assert(alignof(wchar_t) == alignof(char32_t));
    auto error = std::mbrtowc(reinterpret_cast<wchar_t*>(&result), bytes.data(),
                              4, &mb_state);
#else
    auto error = std::mbrtoc32(&result, bytes.data(), 4, &mb_state);
#endif
    if (error == std::size_t(-1))
        throw std::runtime_error{"esc::detail::to_char32_t: Bad Byte Sequence"};
    if (error == std::size_t(-2)) {
        throw std::runtime_error{
            "esc::detail::to_char32_t: Incomplete Byte Sequence"};
    }
    return result;
}

}  // namespace esc::detail
#endif  // ESC_DETAIL_TO_CHAR32_T_HPP
