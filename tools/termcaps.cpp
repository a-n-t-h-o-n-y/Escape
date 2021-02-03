#include <variant>

#include <esc/esc.hpp>

// return false if should exit.
template <typename T>
auto process(T) -> bool
{
    return true;
}

template <>
auto process<esc::Key_press>(esc::Key_press k) -> bool
{
    return k.key != esc::Key::q;
}

// Don't let mouse mode cycle to off, otherwise you can't change the mode.

using namespace esc;

auto constexpr mouse_mode_row          = 0uL;
auto constexpr mouse_input_display_row = 1uL;
auto constexpr key_input_display_row   = 2uL;

auto constexpr brush_1 =
    Brush{Color_index{5}, Color_index{7}, Trait::Bold | Trait::Underline};

auto constexpr brush_2 =
    Brush{True_color{HSL{223, 90, 80}}, True_color{RGB{50, 200, 75}}};

auto constexpr brush_3 = Brush{Yellow, Black};

int main()
{
    initialize_interactive_terminal(Mouse_mode::Move, Signals::Off);

    // Mouse Mode Select
    write(escape(Cursor_position{0, mouse_mode_row}));

    // Mouse Input Display
    write(escape(Cursor_position{0, mouse_input_display_row}));

    // Key Input Display
    write(escape(Cursor_position{0, key_input_display_row}));

    flush();

    write(escape(Cursor_position{1, 5}, brush_1));
    write("Hello, Terminal!");

    write(escape(Cursor_position{1, 6}));
    write(escape(Trait::None));
    write(escape(background(Default_color{})));
    write(escape(foreground(Default_color{})));
    // write(escape(background(True_color{RGB{123, 123, 250}})));
    // write(escape(foreground(True_color{HSL{300, 50, 75}})));
    write("Hello, No Traits!");

    // clang-format off
    write(
        escape(
            brush_2,
            Cursor_position{1, 7}
        ),
        "hello, brush_2!",
        escape(brush_1),
        " and another"
    );
    // clang-format on

    write(escape(Cursor_position{1, 8}, brush_3));
    write("hello, yellow background and black foreground");

    flush();

    while (std::visit([](auto event) { return process(event); }, read())) {}

    uninitialize_terminal();
}
