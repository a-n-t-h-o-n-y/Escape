#include <cstdint>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>

#include <esc/esc.hpp>

using namespace esc;

// clang-format off
[[nodiscard]] auto title_display(Point offset) -> std::string
{
    return
        escape(
            Cursor{offset},
            Trait::Underline
        ) + "~Terminal Capabilities~ -- q to quit";
}

[[nodiscard]] auto TERM_display(Point offset) -> std::string
{
    return
        escape(
            Cursor{offset},
            Trait::Bold
        ) + "TERM env var:        " +
        escape(Trait::None).append(TERM_var());
}

[[nodiscard]] auto color_count_display(Point offset) -> std::string
{
    return
        escape(
            Cursor{offset},
            Trait::Bold
        ) + "Palette Color Count: " +
        escape(Trait::None) + std::to_string(color_palette_size());
}

[[nodiscard]] auto has_true_color_display(Point offset) -> std::string
{
    return
        escape(
            Cursor{offset},
            Trait::Bold
        ) + "Has True Color:      " +
        escape(Trait::None) + (has_true_color() ? "Yes" : "No");
}

[[nodiscard]] auto area_display(Area a) -> std::string
{
    return "{.width = "   + std::to_string(a.width) +
           ", .height = " + std::to_string(a.height) + "}";
}

[[nodiscard]] auto point_display(Point p) -> std::string
{
    return "{.x = " + std::to_string(p.x) + ", .y = " + std::to_string(p.y) + "}";
}

[[nodiscard]] auto term_size_display(Point offset, Area a) -> std::string
{
    return
        escape(
            Cursor{offset},
            Trait::Bold
        ) + "Terminal Size:       " +
        escape(Trait::None) + area_display(a);
}

[[nodiscard]] auto utf8_text_display(Point offset) -> std::string
{
    return
        escape(
            Cursor{offset},
            Trait::Bold
        ) + "UTF8 Text Examples:  " +
        escape(Trait::None) + "Ɣ ѝ ₪ ⌛ ▚ 😳 ㅎꂆ";
}

[[nodiscard]] auto traits_display(Point offset) -> std::string
{
    return
        escape(
            Cursor{offset},
            Trait::Bold
        ) + "Traits:              " +
        escape(Trait::Standout) + "Standout " +
        escape(Trait::Bold) + "Bold " +
        escape(Trait::Dim) + "Dim" +
        escape(Cursor{offset.x, offset.y + 1}) + std::string(21, ' ') +
        escape(Trait::Italic) + "Italic " +
        escape(Trait::Underline) + "Underline " +
        escape(Cursor{offset.x, offset.y + 2}) + escape(Trait::None) + std::string(21, ' ') +
        escape(Trait::Blink) + "Blink" +
        escape(Trait::Inverse) + "Inverse " + escape(Trait::None) +
        escape(Cursor{offset.x, offset.y + 3}) + std::string(21, ' ') +
        escape(Trait::Invisible) + "Invisible " + escape(Trait::None) + "<-- Invisible" +
        escape(Cursor{offset.x, offset.y + 4}) + std::string(21, ' ') +
        escape(Trait::Crossed_out) + "Crossed_out " + escape(Trait::None) +
        escape(Cursor{offset.x, offset.y + 5}) + std::string(21, ' ') +
        escape(Trait::Double_underline) + "Double_underline";
}

[[nodiscard]] auto key_modifiers_display(Key k) -> std::string
{
    auto x = std::string{};
    x.append(is_set(k, Mod::Shift) ? "Shift " : "");
    x.append(is_set(k, Mod::Ctrl) ? "Ctrl " : "");
    x.append(is_set(k, Mod::Alt) ? "Alt " : "");
    x.append(is_set(k, Mod::Meta) ? "Meta" : "");
    return x;
}

[[nodiscard]] auto key_display(Key k) -> std::string
{
    auto const d = key_to_char32(k);
    return detail::u32_to_u8(d == U'\0' ? U' ' : d);
}

[[nodiscard]] auto key_release_display(Point offset, Key k) -> std::string
{
    return
        escape(
            Cursor{offset},
            Trait::Inverse
        ) + "Last Key Release -----" +
        escape(Cursor{offset.x, offset.y + 1}, Trait::Bold) +
        "    Enum Value: " + escape(Trait::None) + std::to_string(static_cast<std::underlying_type_t<Key>>(k)) + "    " +
        escape(Cursor{offset.x, offset.y + 2}, Trait::Bold) +
        "    Display:    " + escape(Trait::None) + key_display(k) +
        escape(Cursor{offset.x, offset.y + 3}, Trait::Bold, BlankRow{}) +
        "    Modifiers:  " + escape(Trait::None) + key_modifiers_display(k);
}

[[nodiscard]] auto key_press_display(Point offset, Key k) -> std::string
{
    return
        escape(
            Cursor{offset},
            Trait::Inverse
        ) + "Last Key Press -----" +
        escape(Cursor{offset.x, offset.y + 1}, Trait::Bold) +
        "    Enum Value: " + escape(Trait::None) + std::to_string(static_cast<std::underlying_type_t<Key>>(k)) + "    " +
        escape(Cursor{offset.x, offset.y + 2}, Trait::Bold) +
        "    Display:    " + escape(Trait::None) + key_display(k) +
        escape(Cursor{offset.x, offset.y + 3}, Trait::Bold, BlankRow{}) +
        "    Modifiers:  " + escape(Trait::None) + key_modifiers_display(k);
}

[[nodiscard]] auto mouse_modifiers_display(Mouse::Modifiers m) -> std::string
{
    auto x = std::string{};
    x.append(m.shift ? "Shift " : "");
    x.append(m.ctrl ? "Ctrl " : "");
    x.append(m.alt ? "Alt" : "");
    return x;
}

[[nodiscard]] auto mouse_display(Point offset, Mouse m, std::string type) -> std::string
{
    return
        escape(
            Cursor{offset},
            Trait::Inverse
        ) + "Last Mouse Event -----" +
        escape(Cursor{offset.x, offset.y + 1}, Trait::Bold, BlankRow{}) +
        "    Event Type: " + escape(Trait::None) + type +
        escape(Cursor{offset.x, offset.y + 2}, Trait::Bold, BlankRow{}) +
        "    Button Name: " + escape(Trait::None) + to_string(m.button) +
        escape(Cursor{offset.x, offset.y + 3}, Trait::Bold) +
        "    at:          " + escape(Trait::None) + point_display(m.at) +
        escape(Cursor{offset.x, offset.y + 4}, Trait::Bold, BlankRow{}) +
        "    Modifiers:   " + escape(Trait::None) + mouse_modifiers_display(m.modifiers);
}

[[nodiscard]] auto mouse_mode_display(Point offset, MouseMode mm) -> std::string
{
    return
        escape(
            Cursor{offset},
            Trait::Bold
        ) + "Click to Change Mouse Mode:" +
        escape(Trait::None) + ' ' + to_string(mm) + ' ';
}

[[nodiscard]] auto color_palette_display(Point offset) -> std::string
{
    constexpr auto width = 36;
    auto result = std::string{};
    for (auto i = 0; i < color_palette_size(); ++i) {
        result.append(escape(Cursor{
                .x = offset.x + (i % width),
                .y = offset.y + (i / width) + 1
            }, bg(XColor{static_cast<std::uint8_t>(i)})));
        result.push_back(' ');
    }
    return result.append(escape(bg(XColor::Default)));
}

[[nodiscard]] auto hue_increment_row(int width, HSL start) -> std::string
{
    auto result = std::string{};
    for (auto i = 0; i < width; ++i) {
        result.append(escape(bg(TrueColor{start})));
        result.push_back(' ');
        start.hue += 1;
    }
    return result;
}

[[nodiscard]] auto saturation_increment_row(int width, HSL start) -> std::string
{
    auto result = std::string{};
    for (auto i = 0; i < width; ++i) {
        result.append(escape(bg(TrueColor{start})));
        result.push_back(' ');
        start.saturation += 1;
    }
    return result;
}

[[nodiscard]] auto lightness_increment_row(int width, HSL start) -> std::string
{
    auto result = std::string{};
    for (auto i = 0; i < width; ++i) {
        result.append(escape(bg(TrueColor{start})));
        result.push_back(' ');
        start.lightness += 1;
    }
    return result;
}

[[nodiscard]] auto true_color_display(Point offset) -> std::string
{
    if (!has_true_color()) {
        return "";
    }
    constexpr auto width = 36;
    auto const init = HSL{100, 50, 50};

    return
        escape(
            Cursor{offset},
            Trait::Bold
        ) + "True Color Example" +
        escape(Cursor{offset.x, offset.y + 1}) +
        hue_increment_row(width, init) +
        escape(Cursor{offset.x, offset.y + 2}) +
        saturation_increment_row(width, init) +
        escape(Cursor{offset.x, offset.y + 3}) +
        lightness_increment_row(width, init) +
        escape(bg(XColor::Default)
    );
}

auto constexpr column_two = 44;

struct {
    Point title{0, 0};
    Point term_display{0, 1};
    Point color_count{0, 2};
    Point has_true_color{0, 3};
    Point term_size{0, 4};
    Point utf8_text{0, 5};
    Point traits_display{0, 6};
    Point key_display{0, 12};
    Point mouse_display{0, 17};
    Point change_mouse_mode{0, 23};
    Point true_color_display{column_two, 0};
    Point color_palette_display{column_two, 5};
} constexpr offset;

[[nodiscard]] auto termcaps_display() -> std::string
{
    return
        title_display(offset.title) +
        TERM_display(offset.term_display) +
        color_count_display(offset.color_count) +
        has_true_color_display(offset.has_true_color) +
        term_size_display(offset.term_size, terminal_area()) +
        utf8_text_display(offset.utf8_text) +
        traits_display(offset.traits_display) +
        key_press_display(offset.key_display, Key::Null) +
        mouse_display(offset.mouse_display, Mouse{{0, 0}, Mouse::Button::None}, "") +
        mouse_mode_display(offset.change_mouse_mode, MouseMode::Basic) +
        true_color_display(offset.true_color_display) +
        color_palette_display(offset.color_palette_display);
}

// clang-format on

/// Returns the next mouse mode, skipping MouseMode::Off
auto next_mouse_mode(MouseMode mm) -> MouseMode
{
    switch (mm) {
        case MouseMode::Move:
        case MouseMode::Off: return MouseMode::Basic;
        case MouseMode::Basic: return MouseMode::Drag;
        case MouseMode::Drag: return MouseMode::Move;
    }
    throw std::runtime_error{"next_mouse_mode(): Bad enum value for mm."};
}

/// std::nullopt is used to indicate a quit request, string is escape seq.
using EventResponse = std::optional<std::string>;

auto process(MousePress m) -> EventResponse
{
    static auto mouse_mode = MouseMode::Basic;

    auto result = std::string{};
    if (m.mouse.at.y == offset.change_mouse_mode.y) {
        mouse_mode = next_mouse_mode(mouse_mode);
        set(mouse_mode);
        result.append(mouse_mode_display(offset.change_mouse_mode, mouse_mode));
    }
    return result.append(
        mouse_display(offset.mouse_display, m.mouse, "MousePress"));
}

auto process(MouseRelease m) -> EventResponse
{
    return mouse_display(offset.mouse_display, m.mouse, "MouseRelease");
}

auto process(MouseWheel m) -> EventResponse
{
    return mouse_display(offset.mouse_display, m.mouse, "MouseWheel");
}

auto process(MouseMove m) -> EventResponse
{
    return mouse_display(offset.mouse_display, m.mouse, "MouseMove");
}

auto process(KeyPress k) -> EventResponse
{
    if (k.key == Key::q) {
        return std::nullopt;
    }
    return key_press_display(offset.key_display, k.key);
}

auto process(KeyRelease kr) -> EventResponse
{
    return key_release_display(offset.key_display, kr.key);
}

auto process(Resize r) -> EventResponse
{
    return term_size_display(offset.term_size, r.area);
}

auto main() -> int
{
    try {
        initialize_interactive_terminal();

        write(termcaps_display());
        flush();

        while (true) {
            auto const r = std::visit([](auto event) { return process(event); },
                                      esc::read());
            if (!r) {
                break;
            }
            write(*r);
            flush();
        }

        uninitialize_terminal();
        return 0;
    }
    catch (std::exception const& e) {
        uninitialize_terminal();
        std::cerr << "Exception: " << e.what() << '\n';
        return 1;
    }
    catch (...) {
        uninitialize_terminal();
        std::cerr << "Unknown Exception\n";
        return 1;
    }
}
