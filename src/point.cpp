#include <esc/point.hpp>

#include <functional>

namespace std {

auto hash<esc::Point>::operator()(argument_type const& point) const noexcept
    -> result_type
{
    auto const h1 = std::hash<decltype(point.x)>{}(point.x);
    auto const h2 = std::hash<decltype(point.y)>{}(point.y);
    return h1 ^ (h2 << 1);
}

}  // namespace std
