#ifndef ESC_DETAIL_PRINT_HPP
#define ESC_DETAIL_PRINT_HPP
#include <ostream>

#include <esc/event.hpp>
#include <esc/key.hpp>
#include <esc/mouse.hpp>

namespace esc::detail {

/// Print Key_press struct display to \p os.
auto print(esc::Key_press k, std::ostream& os) -> std::ostream&;

/// Print Mouse_press struct display to \p os.
auto print(esc::Mouse_press m, std::ostream& os) -> std::ostream&;

/// Print Mouse_release struct display to \p os.
auto print(esc::Mouse_release m, std::ostream& os) -> std::ostream&;

/// Print Scroll_wheel struct display to \p os.
auto print(esc::Scroll_wheel m, std::ostream& os) -> std::ostream&;

/// Print Mouse_move struct display to \p os.
auto print(esc::Mouse_move m, std::ostream& os) -> std::ostream&;

/// Print Window_resize struct display to \p os.
auto print(esc::Window_resize w, std::ostream& os) -> std::ostream&;

}  // namespace esc::detail
#endif  // ESC_DETAIL_PRINT_HPP
