#pragma once

#include <esc/area.hpp>
#include <esc/key.hpp>
#include <esc/keyboard.hpp>
#include <esc/mouse.hpp>
#include <esc/query.hpp>
#include <optional>
#include <string>
#include <variant>

namespace esc {
/// Button press at a zero-based cell position.
struct MousePress {
    Point at;
    MouseButton button;
    Modifiers modifiers = {};
    friend auto operator==(MousePress const&, MousePress const&) -> bool = default;
};

/// Button release; legacy protocols may omit which button was released.
struct MouseRelease {
    Point at;
    std::optional<MouseButton> button;
    Modifiers modifiers = {};
    friend auto operator==(MouseRelease const&, MouseRelease const&) -> bool = default;
};

/// Movement with the held button, if reported.
struct MouseMove {
    Point at;
    std::optional<MouseButton> held_button;
    Modifiers modifiers = {};
    friend auto operator==(MouseMove const&, MouseMove const&) -> bool = default;
};

/// Wheel steps at a cell position: negative dx/dy mean left/up.
struct MouseWheel {
    Point at;
    int dx = 0, dy = 0;
    Modifiers modifiers = {};
    friend auto operator==(MouseWheel const&, MouseWheel const&) -> bool = default;
};

/// Current terminal cell dimensions after a resize.
struct Resize {
    Area size;
    friend auto operator==(Resize const&, Resize const&) -> bool = default;
};

/// Terminal focus gained or lost.
struct Focus {
    bool focused;
    friend auto operator==(Focus const&, Focus const&) -> bool = default;
};

/// Owned paste bytes. first/last delimit the paste; aborted marks an incomplete end.
struct PasteChunk {
    std::string bytes;
    bool first;
    bool last;
    bool aborted = false;
    friend auto operator==(PasteChunk const&, PasteChunk const&) -> bool = default;
};

enum class InputProblem { Unsupported, Malformed, TooLong, Truncated, InvalidUtf8 };

/// Owned unsupported or malformed bytes. final ends a bounded diagnostic unit.
struct UnknownInput {
    std::string bytes;
    InputProblem problem;
    bool final = true;
    friend auto operator==(UnknownInput const&, UnknownInput const&) -> bool = default;
};

/// Input events and unsolicited terminal replies; visit with match().
using Event = std::variant<KeyEvent,
                           MousePress,
                           MouseRelease,
                           MouseMove,
                           MouseWheel,
                           Resize,
                           Focus,
                           PasteChunk,
                           UnknownInput,
                           TerminalReply>;
}  // namespace esc
