#pragma once

#include <esc/capabilities.hpp>
#include "encoding.hpp"

namespace contract {
inline auto full_profile() -> esc::TerminalProfile
{
    return {.control = true,
            .colors = esc::ColorSupport::TrueColor,
            .traits = full_traits(),
            .alternate_screen = true,
            .cursor_visibility = true,
            .mouse_protocol = esc::MouseProtocol::Sgr,
            .maximum_mouse_mode = esc::MouseMode::Move,
            .focus_reporting = true,
            .bracketed_paste = true};
}

}  // namespace contract
