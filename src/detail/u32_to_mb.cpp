#include <esc/detail/u32_to_mb.hpp>

#include <stdexcept>
#include <string>

#include <unicode/ucnv.h>
#include <unicode/unistr.h>
#include <unicode/utypes.h>

namespace esc::detail {

auto u32_to_mb(char32_t c) -> std::string
{
    auto u_str  = icu::UnicodeString(static_cast<UChar32>(c));
    auto result = std::string{};
    u_str.toUTF8String(result);

    auto status = U_ZERO_ERROR;
    if (U_FAILURE(status)) {
        throw std::runtime_error{"Unicode conversion failed"};
    }
    return result;
}

}  // namespace esc::detail
