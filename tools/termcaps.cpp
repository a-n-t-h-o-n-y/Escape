#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>

#include <esc/esc.hpp>

using namespace esc;

struct Title {
    explicit Title(Point offset)
    {
        write(escape(Trait::Underline, Cursor_position{offset}),
              "~Terminal Capabilities~ -- q to quit");
    }
};

struct TERM_display {
    TERM_display(Point offset)
    {
        write(escape(Trait::Bold, Cursor_position{offset}),
              "TERM env var:        ", escape(Trait::None), TERM_var());
    }
};

struct Color_count {
    Color_count(Point offset)
    {
        write(escape(Trait::Bold, Cursor_position{offset}),
              "Palette Color Count: ", escape(Trait::None),
              std::to_string(color_palette_size()));
    }
};

struct Has_true_color {
    Has_true_color(Point offset)
    {
        write(escape(Trait::Bold, Cursor_position{offset}),
              "Has True Color:      ", escape(Trait::None),
              has_true_color() ? "Yes" : "No");
    }
};

class Term_size {
   public:
    Term_size(Point offset) : offset_{offset} { this->update(terminal_area()); }

    void update(Area a) const
    {
        write(escape(Cursor_position{offset_}, Blank_row{}, Trait::Bold),
              "Terminal Size:       ", escape(Trait::None), area_to_string(a));
        flush();
    }

   private:
    Point offset_;

   private:
    static auto area_to_string(Area a) -> std::string
    {
        return "{width: " + std::to_string(a.width) +
               ", height: " + std::to_string(a.height) + "}";
    }
};

struct UTF8_text {
    UTF8_text(Point offset)
    {
        write(escape(Trait::Bold, Cursor_position{offset}),
              "UTF8 Text Examples:  ", escape(Trait::None),
              U"Ɣ ѝ ₪ ⌛ ▚ 😳 ㅎꂆ");
    }
};

struct Traits_display {
    Traits_display(Point offset)
    {
        // clang-format off
        write(
            escape(Trait::Bold, Cursor_position{offset}),
            "Traits:              ",
            escape(Trait::Standout), "Standout ",
            escape(Trait::Bold), "Bold ",
            escape(Trait::Dim), "Dim",
                escape(Cursor_position{offset.x, offset.y + 1}),
            std::string(21, ' '),
            escape(Trait::Italic), "Italic ",
            escape(Trait::Underline), "Underline ",escape(Trait::None),
                escape(Cursor_position{offset.x, offset.y + 2}),
            std::string(21, ' '),
            escape(Trait::Blink), "Blink",
            escape(Trait::Inverse), "Inverse ", escape(Trait::None),
                escape(Cursor_position{offset.x, offset.y + 3}),
            std::string(21, ' '),
            escape(Trait::Invisible), "Invisible ", escape(Trait::None),
                "<-- Invisible",
                escape(Cursor_position{offset.x, offset.y + 4}),
                std::string(21, ' '),
            escape(Trait::Crossed_out), "Crossed_out ", escape(Trait::None),
                escape(Cursor_position{offset.x, offset.y + 5}),
                std::string(21, ' '),
            escape(Trait::Double_underline), "Double_underline"
        );
        // clang-format on
    }
};

class Key_press_display {
   public:
    Key_press_display(Point offset) : offset_{offset}
    {
        this->update(KeyPress{Key::Null});
    }

   public:
    void update(KeyRelease kr) const
    {
        auto mods_to_string = [](auto k) {
            auto result = std::string{};
            result.append(is_set(k, Mod::Shift) ? "Shift " : "");
            result.append(is_set(k, Mod::Ctrl) ? "Ctrl " : "");
            result.append(is_set(k, Mod::Alt) ? "Alt " : "");
            result.append(is_set(k, Mod::Meta) ? "Meta" : "");
            return result;
        };

        auto const display = [&kr] {
            auto const d = key_to_char32(kr.key);
            return d == U'\0' ? U' ' : d;
        }();

        // clang-format off
        write(escape(Cursor_position{offset_}, Trait::Inverse),
            "Last Key Release -----",
            escape(Cursor_position{offset_.x, offset_.y + 1}, Trait::Bold),
            "    Enum Value: ", escape(Trait::None), std::to_string(
                static_cast<std::underlying_type_t<Key>>(kr.key)), "    ",
            escape(Cursor_position{offset_.x, offset_.y + 2}, Trait::Bold),
            "    Display:    ", escape(Trait::None), display,
            escape(Cursor_position{offset_.x, offset_.y + 3}, Trait::Bold,
                Blank_row{}),
            "    Modifiers:  ", escape(Trait::None),
                mods_to_string(kr.key)
        );
        // clang-format on
        flush();
    }

    void update(KeyPress kp) const
    {
        auto mods_to_string = [](auto k) {
            auto result = std::string{};
            result.append(is_set(k, Mod::Shift) ? "Shift " : "");
            result.append(is_set(k, Mod::Ctrl) ? "Ctrl " : "");
            result.append(is_set(k, Mod::Alt) ? "Alt " : "");
            result.append(is_set(k, Mod::Meta) ? "Meta" : "");
            return result;
        };

        auto const display = [&kp] {
            auto const d = key_to_char32(kp.key);
            return d == U'\0' ? U' ' : d;
        }();

        // clang-format off
        write(escape(Cursor_position{offset_}, Trait::Inverse),
            "Last Key Press -----",
            escape(Cursor_position{offset_.x, offset_.y + 1}, Trait::Bold),
            "    Enum Value: ", escape(Trait::None), std::to_string(
                static_cast<std::underlying_type_t<Key>>(kp.key)), "    ",
            escape(Cursor_position{offset_.x, offset_.y + 2}, Trait::Bold),
            "    Display:    ", escape(Trait::None), display,
            escape(Cursor_position{offset_.x, offset_.y + 3}, Trait::Bold,
                Blank_row{}),
            "    Modifiers:  ", escape(Trait::None),
                mods_to_string(kp.key)
        );
        // clang-format on
        flush();
    }

   private:
    Point offset_;
};

class Mouse_display {
   public:
    Mouse_display(Point offset) : offset_{offset}
    {
        this->update("", Mouse{{0, 0}, Mouse::Button::None});
    }

   public:
    void update(std::string const& event_type, Mouse m) const
    {
        auto mods_to_string = [](auto mods) {
            auto result = std::string{};
            result.append(mods.shift ? "Shift " : "");
            result.append(mods.ctrl ? "Ctrl " : "");
            result.append(mods.alt ? "Atl" : "");
            return result;
        };

        auto point_to_string = [](Point p) {
            return "{x: " + std::to_string(p.x) + " y: " + std::to_string(p.y) +
                   '}';
        };

        // clang-format off
        write(escape(Cursor_position{offset_}, Trait::Inverse),
            "Last Mouse Event ---",
            escape(Cursor_position{offset_.x, offset_.y + 1}, Trait::Bold,
                Blank_row{}),
            "    Event Type: ", escape(Trait::None), event_type,
            escape(Cursor_position{offset_.x, offset_.y + 2}, Trait::Bold,
                Blank_row{}),
            "    Button Name: ", escape(Trait::None), to_string(m.button),
            escape(Cursor_position{offset_.x, offset_.y + 3}, Trait::Bold),
            "    at:          ", escape(Trait::None), point_to_string(m.at),
            escape(Cursor_position{offset_.x, offset_.y + 4}, Trait::Bold,
                Blank_row{}),
            "    Modifiers:   ", escape(Trait::None),
                mods_to_string(m.modifiers)
        );
        // clang-format on
        flush();
    }

   private:
    Point offset_;
};

class Change_mouse_mode {
   public:
    Change_mouse_mode(Point offset) : offset_{offset}
    {
        set(MouseMode::Basic);
        this->update(MouseMode::Basic);
    }

   public:
    auto row() const -> int { return offset_.y; }

    auto current_mode() const -> MouseMode { return current_; }

    void update(MouseMode mm)
    {
        set(mm);
        current_ = mm;
        write(escape(Cursor_position{offset_}, Blank_row{},
                     Trait::Bold | Trait::Underline),
              "Click to Change Mouse Mode:", escape(Trait::None), ' ',
              to_string(mm));
        flush();
    }

   private:
    Point offset_;
    MouseMode current_;
};

auto constexpr column_two = 44;

class Color_palette_display {
   public:
    Color_palette_display(Point offset) : offset_{offset}
    {
        write(escape(Cursor_position{offset_}, Trait::Bold,
                     background(DefaultColor{})),
              "Color Palette", escape(Trait::None));
        auto const color_count = color_palette_size();
        for (auto i = 0; i < color_count; ++i) {
            write(escape(Cursor_position{offset_.x + (i % width),
                                         offset_.y + (i / width) + 1},
                         background(ColorIndex{static_cast<std::uint8_t>(i)})),
                  ' ');
        }
        write(escape(background(DefaultColor{})));
    }

   private:
    Point offset_;
    static auto constexpr width = 36;
};

class True_color_display {
   public:
    True_color_display(Point offset) : offset_{offset}
    {
        if (!has_true_color())
            return;
        write(escape(Cursor_position{offset_}, Trait::Bold),
              "True Color Example",
              escape(Cursor_position{offset_.x, offset_.y + 1}));
        auto const init = HSL{100, 50, 50};
        write(hue_increment_row(width, init));
        write(escape(Cursor_position{offset_.x, offset_.y + 2}));
        write(saturation_increment_row(width, init));
        write(escape(Cursor_position{offset_.x, offset_.y + 3}));
        write(lightness_increment_row(width, init));
    }

   private:
    Point offset_;
    static auto constexpr width = 36;

   private:
    static auto hue_increment_row(int width, HSL start) -> std::string
    {
        auto result = std::string{};
        for (auto i = 0; i < width; ++i) {
            start.hue += 1;
            result.append(escape(background(TrueColor{start})));
            result.push_back(' ');
        }
        return result;
    }

    static auto saturation_increment_row(int width, HSL start) -> std::string
    {
        auto result = std::string{};
        for (auto i = 0; i < width; ++i) {
            start.saturation += 1;
            result.append(escape(background(TrueColor{start})));
            result.push_back(' ');
        }
        return result;
    }

    static auto lightness_increment_row(int width, HSL start) -> std::string
    {
        auto result = std::string{};
        for (auto i = 0; i < width; ++i) {
            start.lightness += 1;
            result.append(escape(background(TrueColor{start})));
            result.push_back(' ');
        }
        return result;
    }
};

struct Termcaps_app {
    Title title{{0, 0}};
    TERM_display term_display{{0, 1}};
    Color_count color_count{{0, 2}};
    Has_true_color has_true_color{{0, 3}};
    Term_size term_size{{0, 4}};
    UTF8_text utf8_text{{0, 5}};
    Traits_display traits_display{{0, 6}};
    Key_press_display key_press_display{{0, 12}};
    Mouse_display mouse_display{{0, 17}};
    Change_mouse_mode change_mouse_mode{{0, 23}};
    True_color_display true_color_display{{column_two, 0}};
    Color_palette_display color_palette_display{{column_two, 5}};
};

/// Returns the next mouse mode, skipping MouseMode::Off
auto next_mouse_mode(MouseMode mm) -> MouseMode
{
    switch (mm) {
        case MouseMode::Move:
        case MouseMode::Off: return MouseMode::Basic;
        case MouseMode::Basic: return MouseMode::Drag;
        case MouseMode::Drag: return MouseMode::Move;
    }
    throw std::logic_error{"next_mouse_mode(): Bad enum value for mm."};
}

// return false if should exit.
auto process(MousePress m, Termcaps_app& app) -> bool
{
    if (app.change_mouse_mode.row() == m.mouse.at.y) {
        auto& cmm = app.change_mouse_mode;
        cmm.update(next_mouse_mode(cmm.current_mode()));
    }
    app.mouse_display.update("MousePress", m.mouse);
    return true;
}

auto process(MouseRelease m, Termcaps_app& app) -> bool
{
    app.mouse_display.update("MouseRelease", m.mouse);
    return true;
}

auto process(MouseWheel m, Termcaps_app& app) -> bool
{
    app.mouse_display.update("MouseWheel", m.mouse);
    return true;
}

auto process(MouseMove m, Termcaps_app& app) -> bool
{
    app.mouse_display.update("MouseMove", m.mouse);
    return true;
}

auto process(KeyPress k, Termcaps_app& app) -> bool
{
    if (k.key == Key::q)
        return false;
    app.key_press_display.update(k);
    return true;
}

auto process(KeyRelease kr, Termcaps_app& app) -> bool
{
    app.key_press_display.update(kr);
    return true;
}

auto process(Resize r, Termcaps_app& app) -> bool
{
    app.term_size.update(r.area);
    return true;
}

int main()
{
    try {
        initialize_interactive_terminal();

        auto app = Termcaps_app{};
        flush();

        while (std::visit([&app](auto event) { return process(event, app); },
                          read())) {}
        uninitialize_terminal();
    }
    catch (std::exception const& e) {
        uninitialize_terminal();
        std::cerr << "Exception: " << e.what() << '\n';
    }
    catch (...) {
        uninitialize_terminal();
        std::cerr << "Unknown Exception\n";
    }
}
