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
struct MousePress {
    Point at;
    MouseButton button;
    Modifiers modifiers = {};
    friend auto operator==(MousePress const&, MousePress const&) -> bool = default;
};
struct MouseRelease {
    Point at;
    std::optional<MouseButton> button;
    Modifiers modifiers = {};
    friend auto operator==(MouseRelease const&, MouseRelease const&) -> bool = default;
};
struct MouseMove {
    Point at;
    std::optional<MouseButton> held_button;
    Modifiers modifiers = {};
    friend auto operator==(MouseMove const&, MouseMove const&) -> bool = default;
};
struct MouseWheel {
    Point at;
    int dx = 0, dy = 0;
    Modifiers modifiers = {};
    friend auto operator==(MouseWheel const&, MouseWheel const&) -> bool = default;
};
struct Resize {
    Area size;
    friend auto operator==(Resize const&, Resize const&) -> bool = default;
};
struct Focus {
    bool focused;
    friend auto operator==(Focus const&, Focus const&) -> bool = default;
};
struct PasteChunk {
    std::string bytes;
    bool first;
    bool last;
    bool aborted = false;
    friend auto operator==(PasteChunk const&, PasteChunk const&) -> bool = default;
};
enum class InputProblem { Unsupported, Malformed, TooLong, Truncated, InvalidUtf8 };
struct UnknownInput {
    std::string bytes;
    InputProblem problem;
    bool final = true;
    friend auto operator==(UnknownInput const&, UnknownInput const&) -> bool = default;
};
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
