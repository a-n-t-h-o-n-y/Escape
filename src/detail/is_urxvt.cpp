#include <esc/detail/is_urxvt.hpp>

#include <string_view>

namespace esc::detail {

auto is_urxvt(std::string_view TERM) -> bool
{
    return (TERM.find("rxvt-unicode") != std::string_view::npos);
}

}  // namespace esc::detail
