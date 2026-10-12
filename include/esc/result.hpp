#pragma once
#include <utility>
#include <variant>

namespace esc {
struct Timeout {
    friend constexpr bool operator==(Timeout, Timeout) = default;
};
struct EndOfInput {
    friend constexpr bool operator==(EndOfInput, EndOfInput) = default;
};
struct InputPending {
    friend constexpr bool operator==(InputPending, InputPending) = default;
};
namespace detail {
template <class... Functions>
struct Overload : Functions... {
    using Functions::operator()...;
};
template <class... Functions>
Overload(Functions...) -> Overload<Functions...>;
}  // namespace detail

// Exhaustive visitation with the variant's normal reference and return semantics.
template <class Variant, class... Functions>
decltype(auto) match(Variant&& value, Functions&&... functions)
{
    return std::visit(detail::Overload{std::forward<Functions>(functions)...},
                      std::forward<Variant>(value));
}
}  // namespace esc
