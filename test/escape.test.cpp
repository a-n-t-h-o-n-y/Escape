#include <iostream>

#include <esc/debug.hpp>
#include <esc/esc.hpp>
#include <esc/io.hpp>
#include "esc/color_index.hpp"
#include "esc/true_color.hpp"

int main()
{
    esc::io::initialize();

    esc::io::write(esc::move_cursor({1, 1}));
    esc::io::write(esc::set_foreground(esc::Color_index{9}));
    esc::io::write(esc::set_background(esc::Color_index{5}));
    esc::io::write(
        esc::set_traits(esc::Trait::Double_underline, esc::Trait::Italic));
    esc::io::write(U"~~~~~~~~~~~~");
    esc::io::write(esc::move_cursor({1, 0}));
    esc::io::write(esc::set_background(esc::True_color{esc::RGB{0, 255, 0}}));
    esc::io::write(esc::set_foreground(esc::True_color{esc::HSL{274, 62, 80}}));
    esc::io::write("Hello, World!");
    esc::io::write(esc::set_traits(esc::Trait::Standout));
    esc::io::write(esc::move_cursor({1, 2}));
    esc::io::write("Hello, ѱ!");
    esc::io::write(esc::move_cursor({1, 3}));
    esc::io::write(esc::clear_traits());
    esc::io::write(U'ↂ');
    esc::io::write(esc::reset_background());
    esc::io::write(U'x');
    esc::io::write(esc::move_cursor({1, 4}));
    esc::io::write(U"abcѮ Ȭ ⏳xyz");

    esc::io::write(esc::reset_colors());
    esc::io::write(esc::clear_traits());
    esc::io::flush();

    while (true) {
        std::visit([](auto r) { esc::debug::print(r, std::cerr); },
                   esc::io::read());
    }

    esc::io::uninitialize();
}
