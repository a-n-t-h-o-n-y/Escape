#ifndef ESC_EVENT_HPP
#define ESC_EVENT_HPP
#include <cstddef>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <variant>

#include <esc/area.hpp>
#include <esc/key.hpp>
#include <esc/mouse.hpp>
#include <esc/point.hpp>

namespace esc {

/// Mouse Button Pressed
struct Mouse_press {
    Mouse state;
};

/// Mouse Button Released
struct Mouse_release {
    Mouse state;
};

/// Mouse Wheel Scrolled
struct Scroll_wheel {
    Mouse state;
};

/// Mouse Pointer Moved
struct Mouse_move {
    Mouse state;
};

/// Key Press Event
struct Key_press {
    Key key;
};

/// Terminal Window Resized
struct Window_resize {
    Area new_dimensions;
};

/// Any input event type that can be returned from esc::read().
using Event = std::variant<Mouse_press,
                           Mouse_release,
                           Scroll_wheel,
                           Mouse_move,
                           Key_press,
                           Window_resize>;

}  // namespace esc
#endif  // ESC_EVENT_HPP
