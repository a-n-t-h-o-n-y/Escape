#ifndef ESC_IO_HPP
#define ESC_IO_HPP
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

#include <esc/detail/is_convertible_to_any_of.hpp>
#include <esc/event.hpp>
#include <esc/terminfo.hpp>

namespace esc {

// -------------------------- Write to Screen ----------------------------------

/// Writes a single byte to the console via stdout.
void write(char c);

/// Writes a 4 byte char to the console via stdout.
void write(char32_t c) noexcept(false);

/// Writes each char of \p sv to the console via stdout.
void write(std::string_view sv);

/// Writes each char of \p s to the console via stdout.
auto write(std::string const& s) -> void;

/// Writes each char of \p s to the console via stdout.
void write(char const* s);

/// Writes each char32_t of \p sv to the console via stdout.
/** Calls write(char32_t) for each element of \p sv. */
void write(std::u32string_view sv);

// CONVENIENCE -----------------------------------------------------------------

// TODO make concept
template <typename T>
bool constexpr is_writable =
    detail::is_convertible_to_any_of<T,
                                     char,
                                     char32_t,
                                     std::string,
                                     std::string_view,
                                     char const*,
                                     std::u32string_view>;

/// Writes any number of writable objects, in paramter order.
template <typename... Args,
          typename = std::enable_if_t<(sizeof...(Args) > 1), void>>
void write(Args&&... args)
{
    static_assert((is_writable<Args> && ...),
                  "write(...): All Args... must be writable types.");
    (write(args), ...);
}

/// Send all buffered bytes from calls to write(...) to the console device.
void flush();

// ----------------------------- Reading --------------------------------------

/// Blocking read of a single input Event from stdin.
auto read() -> Event;

/// Read a single input Event from stdin, with timeout.
/** Returns std::nullopt if timeout passes without an Event. */
auto read(int millisecond_timeout) -> std::optional<Event>;

}  // namespace esc
#endif  // ESC_IO_HPP
