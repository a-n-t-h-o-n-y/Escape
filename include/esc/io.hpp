#pragma once

#include <concepts>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

#include <esc/event.hpp>

namespace esc {

// -------------------------- Write to Screen ----------------------------------

/**
 * Write a single byte to the console via stdout.
 * @param c The char to write.
 */
auto write(char c) -> void;

/**
 * Write a 4 byte char32_t to the console via stdout.
 * @param c The char to write.
 */
auto write(char32_t c) -> void;

/**
 * Write a string_view to the console via stdout.
 * @param sv The string_view to write.
 */
auto write(std::string_view sv) -> void;

/**
 * Write a string to the console via stdout.
 * @details This is an optimization over the std::string_view overload since
 *          null terminators are required in std::string but not in
 *          std::string_view.
 * @param s The string to write.
 */
auto write(std::string const& s) -> void;

/**
 * Write a null-terminated string to the console via stdout.
 * @param s The string to write.
 */
auto write(char const* s) -> void;

/**
 * Write a u32string_view to the console via stdout.
 * @details Calls write(char32_t) for each element of \p sv.
 * @param sv The u32string_view to write.
 */
auto write(std::u32string_view sv) -> void;

// CONVENIENCE -----------------------------------------------------------------

template <typename T>
concept Writable = std::convertible_to<T, char32_t> ||
                   std::convertible_to<T, std::string_view> ||
                   std::convertible_to<T, std::u32string_view>;

/**
 * Write any number of writable objects to the console, in parameter order.
 * @tparam Args The types of the objects to write.
 * @param args The objects to write.
 */
template <Writable... Args,
          typename = std::enable_if_t<(sizeof...(Args) > 1), void>>
auto write(Args&&... args) -> void
{
    (write(args), ...);
}

/**
 * Flush the stdout buffer.
 * @details Sends all buffered bytes from calls to write(...) to the console.
 */
auto flush() -> void;

// ----------------------------- Reading --------------------------------------

/**
 * Blocks until a single input Event is read from stdin.
 * @return The Event read from stdin.
 */
auto read() -> Event;

/**
 * Timeout version of read().
 * @param millisecond_timeout The maximum time to wait for input.
 * @return The Event read from stdin, or std::nullopt if the timeout is reached.
 */
auto read(int millisecond_timeout) -> std::optional<Event>;

}  // namespace esc