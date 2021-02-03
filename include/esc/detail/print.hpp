#ifndef ESC_DETAIL_PRINT_HPP
#define ESC_DETAIL_PRINT_HPP
#include <cstdint>
#include <ios>
#include <iostream>
#include <sstream>
#include <string>

#include <esc/event.hpp>
#include <esc/key.hpp>
#include <esc/mouse.hpp>

namespace esc::detail {

/// Print Key_press struct display to \p os.
inline auto print(esc::Key_press k, std::ostream& os) -> std::ostream&
{
    os << "Key::" << static_cast<std::uint32_t>(k.key) << '\n'
       << std::boolalpha << "shift: " << k.modifiers.shift << '\n'
       << "ctrl: " << k.modifiers.ctrl << '\n'
       << "alt: " << k.modifiers.alt << '\n';
    return os;
}

/// Display Mouse as text, prefix is applied before each line.
[[nodiscard]] inline auto to_string(esc::Mouse m, std::string prefix)
    -> std::string
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

/// Print Mouse_press struct display to \p os.
inline auto print(esc::Mouse_press m, std::ostream& os) -> std::ostream&
{
    os << "struct Mouse_press {\n" << to_string(m.state, "\t") << "\n};\n";
    return os;
}

/// Print Mouse_release struct display to \p os.
inline auto print(esc::Mouse_release m, std::ostream& os) -> std::ostream&
{
    os << "struct Mouse_release {\n" << to_string(m.state, "\t") << "\n};\n";
    return os;
}

/// Print Scroll_wheel struct display to \p os.
inline auto print(esc::Scroll_wheel m, std::ostream& os) -> std::ostream&
{
    os << "struct Scroll_wheel {\n" << to_string(m.state, "\t") << "\n};\n";
    return os;
}

/// Print Mouse_move struct display to \p os.
inline auto print(esc::Mouse_move m, std::ostream& os) -> std::ostream&
{
    os << "struct Mouse_move {\n" << to_string(m.state, "\t") << "\n};\n";
    return os;
}

/// Print Window_resize struct display to \p os.
inline auto print(esc::Window_resize w, std::ostream& os) -> std::ostream&
{
    os << "struct Window_resize {\n"
       << "    Area{width: " << w.new_dimensions.width
       << " height: " << w.new_dimensions.height << "}\n"
       << "};\n";
    return os;
}

}  // namespace esc::detail
#endif  // ESC_DETAIL_PRINT_HPP
