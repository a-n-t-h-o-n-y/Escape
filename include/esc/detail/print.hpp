#pragma once

#include <ostream>

#include <esc/event.hpp>
#include <esc/key.hpp>
#include <esc/mouse.hpp>

namespace esc::detail {

/// Print KeyPress struct display to \p os.
auto print(esc::KeyPress k, std::ostream& os) -> std::ostream&;

/// Print MousePress struct display to \p os.
auto print(esc::MousePress m, std::ostream& os) -> std::ostream&;

/// Print MouseRelease struct display to \p os.
auto print(esc::MouseRelease m, std::ostream& os) -> std::ostream&;

/// Print MouseWheel struct display to \p os.
auto print(esc::MouseWheel m, std::ostream& os) -> std::ostream&;

/// Print MouseMove struct display to \p os.
auto print(esc::MouseMove m, std::ostream& os) -> std::ostream&;

/// Print Resize struct display to \p os.
auto print(esc::Resize w, std::ostream& os) -> std::ostream&;

}  // namespace esc::detail