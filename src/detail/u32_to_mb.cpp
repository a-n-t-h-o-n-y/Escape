#include <esc/detail/u32_to_mb.hpp>

#include <array>
#include <cstddef>
#include <stdexcept>
#include <utility>

#ifdef __APPLE__
#    include <cwchar>
#else
#    include <cuchar>
#endif

namespace esc::detail {

auto u32_to_mb(char32_t c) -> std::pair<std::size_t, std::array<char, 4>>
{
    auto result = std::array<char, 4>{};
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
        throw std::runtime_error{"u32_to_mb(char32_t): Error in Conversion."};
    return {count, result};
}

}  // namespace esc::detail
