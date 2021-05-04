#include <esc/detail/display.hpp>

#include <string>

namespace esc::detail {

auto display(char c) -> std::string
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

auto display(std::string const& sequence) -> std::string
{
    auto result = std::string{};
    for (auto c : sequence)
        result.append(display(c));
    return result;
}


}  // namespace esc::detail
