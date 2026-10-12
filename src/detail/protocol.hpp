#pragma once

#include <esc/event.hpp>
#include <string_view>

namespace esc::detail {

auto parse_reply(std::string_view fields, char final) -> std::optional<Reply>;

auto parse_kitty(std::string_view fields, char final) -> std::optional<KeyEvent>;

void validate_kitty_flags(KittyFlags flags);
}  // namespace esc::detail
