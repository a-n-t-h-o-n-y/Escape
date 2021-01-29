#include <sstream>
#include <string>
#include <variant>

#include <esc/detail/debug.hpp>
#include <esc/esc.hpp>
#include "esc/sequence.hpp"

int main()
{
    esc::io::initialize();

    esc::io::write(esc::set_mouse_mode(esc::Mouse_mode::Basic));
    esc::io::write(esc::alternate_screen_buffer());

    esc::io::write(esc::hide_cursor());

    esc::io::write(esc::move_cursor({1, 0}));
    esc::io::write(esc::set_foreground(esc::Color_index{9}));
    esc::io::write(esc::set_background(esc::Color_index{5}));
    esc::io::write(
        esc::set_traits(esc::Trait::Double_underline | esc::Trait::Italic));
    esc::io::write(U"Bright Red Foreground, Magenta Background; ");
    esc::io::write(U"Double Underline, Italic");

    esc::io::write(esc::move_cursor({1, 1}));
    esc::io::write(esc::set_foreground(esc::True_color{esc::HSL{274, 62, 80}}));
    esc::io::write(esc::set_background(esc::True_color{esc::RGB{0, 200, 0}}));
    esc::io::write("Light Purple Foreground, Lime Background; ");
    esc::io::write(U"Double Underline, Italic");

    esc::io::write(esc::set_traits(esc::get_traits() | esc::Trait::Bold));
    esc::io::write(esc::move_cursor({1, 2}));
    esc::io::write("Light Purple Foreground, Lime Background; ");
    esc::io::write("Standout ѱ");

    esc::io::write(esc::set_traits(esc::get_traits().remove(esc::Trait::Bold)));
    esc::io::write(esc::move_cursor({1, 3}));
    esc::io::write(esc::clear_traits());
    esc::io::write("Light Purple Foreground, Lime Background; ");
    esc::io::write("No Traits; unicode: ");
    esc::io::write(U'ↂ');

    esc::io::write(esc::reset_background());
    esc::io::write(esc::move_cursor({1, 4}));
    esc::io::write("Light Purple Foreground, Default Background; ");
    esc::io::write("No Traits ");
    esc::io::write(U'x');

    esc::io::write(esc::move_cursor({1, 5}));
    esc::io::write("Light Purple Foreground, Default Background; ");
    esc::io::write("No Traits ");
    esc::io::write(U"abcѮ Ȭ ⏳xyz");

    esc::io::write(esc::move_cursor({1, 6}));
    esc::io::write("${TERM}: " + std::string{esc::get_TERM()});

    esc::io::write(esc::move_cursor({1, 7}));
    esc::io::write("Color Palette Size: " +
                   std::to_string(esc::color_palette_size()));

    esc::io::write(esc::move_cursor({1, 8}));
    esc::io::write("has_true_color: " + std::to_string(esc::has_true_color()));

    esc::io::write(esc::reset_colors());
    esc::io::write(esc::clear_traits());
    esc::io::flush();

    while (true) {
        std::visit(
            [](auto r) {
                auto ss = std::ostringstream{};
                esc::detail::print(r, ss);
                esc::io::write(ss.str());
                esc::io::flush();
            },
            esc::io::read());
    }

    esc::io::write(esc::set_mouse_mode(esc::Mouse_mode::Off));
    esc::io::write(esc::normal_screen_buffer());
    esc::io::uninitialize();
}
