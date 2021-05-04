#include <esc/mouse.hpp>

#include <cstdint>
#include <stdexcept>
#include <string>

namespace esc {

auto to_string(Mouse::Button b) -> std::string
{
    auto const btn = std::string{"Mouse::Button::"};
    switch (b) {
        case Mouse::Button::Left: return btn + "Left";
        case Mouse::Button::Middle: return btn + "Middle";
        case Mouse::Button::Right: return btn + "Right";
        case Mouse::Button::None: return btn + "None";
        case Mouse::Button::ScrollUp: return btn + "ScrollUp";
        case Mouse::Button::ScrollDown: return btn + "ScrollDown";
        case Mouse::Button::Six: return btn + "Six";
        case Mouse::Button::Seven: return btn + "Seven";
        case Mouse::Button::Eight: return btn + "Eight";
        case Mouse::Button::Nine: return btn + "Nine";
        case Mouse::Button::Ten: return btn + "Ten";
        case Mouse::Button::Eleven: return btn + "Eleven";
    }
    throw std::runtime_error{"Invalid Mouse::Button enum value: " +
                             std::to_string(static_cast<std::uint16_t>(b))};
}

auto to_string(Mouse_mode mm) -> std::string
{
    switch (mm) {
        case Mouse_mode::Off: return "Mouse_mode::Off";
        case Mouse_mode::Basic: return "Mouse_mode::Basic";
        case Mouse_mode::Drag: return "Mouse_mode::Drag";
        case Mouse_mode::Move: return "Mouse_mode::Move";
    }
    throw std::logic_error{"to_string(Mouse_mode): Bad Enum Value."};
}

}  // namespace esc
