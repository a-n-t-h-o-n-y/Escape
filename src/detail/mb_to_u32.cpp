#include <esc/detail/mb_to_u32.hpp>

#include <array>
#include <cstddef>
#include <stdexcept>

#ifdef __APPLE__
#    include <cwchar>
#else
#    include <cuchar>
#endif

namespace esc::detail {

auto mb_to_u32(std::array<char, 4> bytes) -> char32_t
{
    auto result   = char32_t{};
    auto mb_state = std::mbstate_t{};
#ifdef __APPLE__
    static_assert(sizeof(wchar_t) == sizeof(char32_t));
    static_assert(alignof(wchar_t) == alignof(char32_t));
    auto const error = std::mbrtowc(reinterpret_cast<wchar_t*>(&result),
                                    bytes.data(), 4, &mb_state);
#else
    auto const error = std::mbrtoc32(&result, bytes.data(), 4, &mb_state);
#endif
    if (error == std::size_t(-1))
        throw std::runtime_error{"esc::detail::mb_to_u32: Bad Byte Sequence"};
    if (error == std::size_t(-2)) {
        throw std::runtime_error{
            "esc::detail::mb_to_u32: Incomplete Byte Sequence"};
    }
    return result;
}

}  // namespace esc::detail
