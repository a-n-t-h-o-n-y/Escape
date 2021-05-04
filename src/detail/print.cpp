#include <esc/detail/print.hpp>

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
       << std::boolalpha
       // clang-format off
       << prefix << "\t\tbool shift = " << m.modifiers.shift << '\n'
       << prefix << "\t\tbool ctrl  = " << m.modifiers.ctrl << '\n'
       << prefix << "\t\tbool alt   = " << m.modifiers.alt << '\n'
       // clang-format on
       << prefix << "\t}\n"
       << prefix << "};\n";
    return ss.str();
}

}  // namespace

namespace esc::detail {

auto print(esc::Key_press k, std::ostream& os) -> std::ostream&
{
    os << "Key::" << static_cast<std::uint32_t>(k.key) << '\n'
       << std::boolalpha << "shift: " << is_set(k.key, esc::Mod::Shift) << '\n'
       << "ctrl: " << is_set(k.key, esc::Mod::Ctrl) << '\n'
       << "alt: " << is_set(k.key, esc::Mod::Alt) << '\n'
       << "meta: " << is_set(k.key, esc::Mod::Meta) << '\n';
    return os;
}

auto print(esc::Mouse_press m, std::ostream& os) -> std::ostream&
{
    os << "struct Mouse_press {\n" << ::to_string(m.state, "\t") << "\n};\n";
    return os;
}

auto print(esc::Mouse_release m, std::ostream& os) -> std::ostream&
{
    os << "struct Mouse_release {\n" << ::to_string(m.state, "\t") << "\n};\n";
    return os;
}

auto print(esc::Scroll_wheel m, std::ostream& os) -> std::ostream&
{
    os << "struct Scroll_wheel {\n" << ::to_string(m.state, "\t") << "\n};\n";
    return os;
}

auto print(esc::Mouse_move m, std::ostream& os) -> std::ostream&
{
    os << "struct Mouse_move {\n" << ::to_string(m.state, "\t") << "\n};\n";
    return os;
}

auto print(esc::Window_resize w, std::ostream& os) -> std::ostream&
{
    os << "struct Window_resize {\n"
       << "    Area{width: " << w.new_dimensions.width
       << " height: " << w.new_dimensions.height << "}\n"
       << "};\n";
    return os;
}

}  // namespace esc::detail
