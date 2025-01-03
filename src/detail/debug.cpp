#include <esc/detail/debug.hpp>

#include <cstdint>
#include <ios>
#include <ostream>
#include <sstream>
#include <string>

#include <esc/event.hpp>
#include <esc/key.hpp>
#include <esc/mouse.hpp>

namespace {

[[nodiscard]] auto to_string(esc::Mouse m, std::string prefix) -> std::string
{
    auto ss = std::stringstream{};
    ss << prefix << "struct Mouse {\n"
       << prefix << "\tPoint at = {x:" << m.at.x << " y:" << m.at.y << "}\n"
       << prefix << "\tButton button = " << to_string(m.button) << '\n'
       << prefix << "\tmodifiers: {\n"
       << std::boolalpha << prefix << "\t\tbool shift = " << m.modifiers.shift << '\n'
       << prefix << "\t\tbool ctrl  = " << m.modifiers.ctrl << '\n'
       << prefix << "\t\tbool alt   = " << m.modifiers.alt << '\n'
       << prefix << "\t}\n"
       << prefix << "};\n";
    return ss.str();
}

}  // namespace

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
        default: return {c};
    }
}

auto display(std::string const& sequence) -> std::string
{
    auto result = std::string{};
    for (auto c : sequence) {
        result.append(display(c));
    }
    return result;
}

auto print(::esc::KeyPress k, std::ostream& os) -> std::ostream&
{
    os << "Key::" << static_cast<std::uint32_t>(k.key) << '\n'
       << std::boolalpha << "shift: " << is_set(k.key, ::esc::Mod::Shift) << '\n'
       << "ctrl: " << is_set(k.key, ::esc::Mod::Ctrl) << '\n'
       << "alt: " << is_set(k.key, ::esc::Mod::Alt) << '\n'
       << "meta: " << is_set(k.key, ::esc::Mod::Meta) << '\n';
    return os;
}

auto print(::esc::MousePress m, std::ostream& os) -> std::ostream&
{
    os << "struct MousePress {\n" << ::to_string(m.mouse, "\t") << "\n};\n";
    return os;
}

auto print(::esc::MouseRelease m, std::ostream& os) -> std::ostream&
{
    os << "struct MouseRelease {\n" << ::to_string(m.mouse, "\t") << "\n};\n";
    return os;
}

auto print(::esc::MouseWheel m, std::ostream& os) -> std::ostream&
{
    os << "struct MouseWheel {\n" << ::to_string(m.mouse, "\t") << "\n};\n";
    return os;
}

auto print(::esc::MouseMove m, std::ostream& os) -> std::ostream&
{
    os << "struct MouseMove {\n" << ::to_string(m.mouse, "\t") << "\n};\n";
    return os;
}

auto print(::esc::Resize r, std::ostream& os) -> std::ostream&
{
    os << "struct Resize {\n"
       << "    Area{width: " << r.size.width << " height: " << r.size.height << "}\n"
       << "};\n";
    return os;
}

}  // namespace esc::detail
