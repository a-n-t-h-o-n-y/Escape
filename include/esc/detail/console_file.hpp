#ifndef ESC_DETAIL_CONSOLE_FILE_HPP
#define ESC_DETAIL_CONSOLE_FILE_HPP
namespace esc::detail {

/// Try to open the current console device as a file. Returns file descriptor.
/** Throws std::runtime_error if file cannot be opened, typically need superuser
 *  privileges to open.*/
[[nodiscard]] auto open_console_file() noexcept(false) -> int;

/// Return the current keyboard mode from ioctl for given \p file_descriptor.
/** Throws std::runtime_error if mode cannot be determined. */
[[nodiscard]] auto current_keyboard_mode(int file_descriptor) noexcept(false)
    -> long;

/// Set given \p mode to \p file_descriptor.
/** Throws std::runtime_error if this fails.
 *  Mode can any of: K_RAW, K_XLATE, K_MEDIUMRAW, K_UNICODE, K_OFF */
void set_keyboard_mode(int file_descriptor, long mode) noexcept(false);

}  // namespace esc::detail
#endif  // ESC_DETAIL_CONSOLE_FILE_HPP
