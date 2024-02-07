#pragma once

#include <variant>

#include <esc/area.hpp>
#include <esc/key.hpp>
#include <esc/mouse.hpp>

namespace esc {

/**
 * Mouse button pressed event.
 */
struct MousePress {
    Mouse mouse;
};

/**
 * Mouse button released event.
 */
struct MouseRelease {
    Mouse mouse;
};

/**
 * Mouse wheel scrolled event.
 */
struct MouseWheel {
    Mouse mouse;
};

/**
 * Mouse pointer moved event.
 */
struct MouseMove {
    Mouse mouse;
};

/**
 * Key press event.
 */
struct KeyPress {
    Key key;
};

/**
 * Key release event.
 * @details Only used with KeyMode::Alternate set.
 */
struct KeyRelease {
    Key key;
};

/**
 * Terminal window resized event.
 */
struct Resize {
    Area area;
};

/**
 * Any input event type that can be returned from esc::read().
 */
using Event = std::variant<MousePress,
                           MouseRelease,
                           MouseWheel,
                           MouseMove,
                           KeyPress,
                           KeyRelease,
                           Resize>;

}  // namespace esc