#pragma once

namespace esc::detail {

/**
 * Try to open the current console device as a file. Returns file descriptor.
 * @return The file descriptor for the console file.
 * @throws std::runtime_error if file cannot be opened, typically need superuser
 * privileges to open.
 */
[[nodiscard]] auto open_console_file() -> int;

/**
 * Return the current keyboard mode from ioctl for given \p file_descriptor.
 * @param file_descriptor The file descriptor to get the mode from.
 * @return The current keyboard mode. Can be any of: K_RAW, K_XLATE, K_MEDIUMRAW,
 * K_UNICODE, K_OFF.
 * @throws std::runtime_error if this fails.
 */
[[nodiscard]] auto current_keyboard_mode(int file_descriptor) -> long;

/**
 * Set given \p mode to \p file_descriptor.
 * @param file_descriptor The file descriptor to set the mode on.
 * @param mode The mode to set. Can be any of: K_RAW, K_XLATE, K_MEDIUMRAW, K_UNICODE,
 * K_OFF.
 * @throws std::runtime_error if this fails.
 */
void set_keyboard_mode(int file_descriptor, long mode);

}  // namespace esc::detail