#ifndef ESC_DEBUG_HPP
#define ESC_DEBUG_HPP
#include <cstdint>
#include <ios>
#include <iostream>
#include <sstream>
#include <string>

#include <esc/event.hpp>
#include <esc/io.hpp>  //temp
#include <esc/key.hpp>

namespace esc::debug {

// char version
/// Translate a single non-visible character into a visible string.
/** Visible chars pass through unchanged, into a string. */
[[nodiscard]] inline auto display(char c) -> std::string
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

/// Translate any control characters to a visible byte string.
/** Visible chars pass through unchanged. */
[[nodiscard]] inline auto display(std::string const& sequence) -> std::string
{
    auto result = std::string{};
    for (auto c : sequence)
        result.append(display(c));
    return result;
}

// -----------------------------------------------------------------------------

inline auto print(esc::Key_press k, std::ostream& os) -> std::ostream&
{
    // if (k.key == esc::Key::Enter)
    //     esc::io::write('\n');
    // else
    //     esc::io::write(key_to_char32(k.key));
    // esc::io::flush();
    // return os;
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

inline auto print(esc::Mouse_press m, std::ostream& os) -> std::ostream&
{
    os << "struct Mouse_press {\n" << to_string(m.state, "\t") << "\n};\n";
    return os;
}

inline auto print(esc::Mouse_release m, std::ostream& os) -> std::ostream&
{
    os << "struct Mouse_release {\n" << to_string(m.state, "\t") << "\n};\n";
    return os;
}

inline auto print(esc::Scroll_wheel m, std::ostream& os) -> std::ostream&
{
    os << "struct Scroll_wheel {\n" << to_string(m.state, "\t") << "\n};\n";
    return os;
}

inline auto print(esc::Mouse_move m, std::ostream& os) -> std::ostream&
{
    os << "struct Mouse_move {\n" << to_string(m.state, "\t") << "\n};\n";
    return os;
}

inline auto print(esc::Window_resize w, std::ostream& os) -> std::ostream&
{
    os << "struct Window_resize {\n"
       << "    Area{width: " << w.new_dimensions.width
       << " height: " << w.new_dimensions.height << "}\n"
       << "};\n";
    return os;
}

// TODO remove None
inline auto print(esc::None, std::ostream& os) -> std::ostream&
{
    os << "No Event\n";
    return os;
}

}  // namespace esc::debug
#endif  // ESC_DEBUG_HPP
