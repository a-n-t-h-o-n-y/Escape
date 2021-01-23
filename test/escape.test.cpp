#include <iostream>

#include <esc/debug.hpp>
#include <esc/esc.hpp>
#include <esc/io.hpp>

int main()
{
    esc::io::initialize();

    esc::io::write(esc::move_cursor({1, 1}));
    esc::io::write(esc::set_traits(esc::Trait::Underline, esc::Trait::Italic));
    esc::io::write("Hello, World!");
    esc::io::write(esc::set_traits(esc::Trait::Standout));
    esc::io::write(esc::move_cursor({1, 2}));
    esc::io::write("Hello, ѱ!");
    esc::io::write(esc::move_cursor({1, 3}));
    esc::io::write(esc::clear_traits());
    esc::io::write(U'ↂ');
    esc::io::write(U'x');
    esc::io::write(esc::move_cursor({1, 4}));
    esc::io::write(U"abcѮ Ȭ ⏳xyz");
    esc::io::flush();

    while (true) {
        std::visit([](auto r) { esc::debug::print(r, std::cerr); },
                   esc::io::read());
    }

    esc::io::uninitialize();
}
