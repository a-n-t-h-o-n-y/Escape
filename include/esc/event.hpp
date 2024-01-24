#pragma once

#include <variant>

#include <esc/area.hpp>
#include <esc/key.hpp>
#include <esc/mouse.hpp>

namespace esc {

/// Mouse Button Pressed
struct MousePress {
    Mouse mouse;
};

/// Mouse Button Released
struct MouseRelease {
    Mouse mouse;
};

/// Mouse Wheel Scrolled
struct MouseWheel {
    Mouse mouse;
};

/// Mouse Pointer Moved
struct MouseMove {
    Mouse mouse;
};

/// Key Press Event
struct KeyPress {
    Key key;
};

/// Key Release Event
/** Only used with Key_mode::Alternate set. */
struct KeyRelease {
    Key key;
};

/// Terminal Window Resized
struct Resize {
    Area area;
};

/// Any input event type that can be returned from esc::read().
using Event = std::variant<MousePress,
                           MouseRelease,
                           MouseWheel,
                           MouseMove,
                           KeyPress,
                           KeyRelease,
                           Resize>;

}  // namespace esc