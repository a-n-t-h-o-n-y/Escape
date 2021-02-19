#ifndef ESC_IO_HPP
#define ESC_IO_HPP
#include <array>
#include <climits>
#include <clocale>
#include <cstddef>
#include <cstdio>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include <esc/detail/is_convertible_to_any_of.hpp>
#include <esc/detail/is_urxvt.hpp>
#include <esc/detail/u32_to_mb.hpp>
#include <esc/event.hpp>
#include <esc/terminfo.hpp>

namespace esc {

// -------------------------- Write to Screen ----------------------------------

/// Writes a single byte to the console via stdout.
inline void write(char c) { std::putchar(c); }

/// Writes a 4 byte char to the console via stdout.
/** Converts the character to a multi-byte array via std::c32rtomb() and sends
 *  each resulting char to write(). The conversion uses the current C locale.
 *  Throws std::runtime_error if there is an error during conversion. */
inline void write(char32_t c) noexcept(false)
{
    auto const [count, chars] = esc::detail::u32_to_mb(c);
    for (auto i = std::size_t{0}; i < count; ++i)
        write(chars[i]);
}

/// Writes each char of \p sv to the console via stdout.
inline auto write(std::string_view sv) -> void
{
    for (auto c : sv)
        write(c);
}

/// Writes each char of \p s to the console via stdout.
inline auto write(std::string const& s) -> void
{
    std::fputs(s.c_str(), stdout);
}

/// Writes each char of \p s to the console via stdout.
inline auto write(char const* s) -> void { std::fputs(s, stdout); }

/// Writes each char32_t of \p sv to the console via stdout.
/** Calls write(char32_t) for each element of \p sv. */
inline void write(std::u32string_view sv)
{
    for (auto c : sv)
        write(c);
}

// CONVENIENCE -----------------------------------------------------------------

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
inline void flush() { std::fflush(::stdout); }

// ----------------------------- Reading --------------------------------------

/// Blocking read of a single input Event from stdin.
auto read() -> Event;

/// Read a single input Event from stdin, with timeout.
/** Returns std::nullopt if timeout passes without an Event. */
auto read(int millisecond_timeout) -> std::optional<Event>;

namespace detail {

/// register handler for terminal window resize event signals.
void register_SIGWINCH();

}  // namespace detail
}  // namespace esc
#endif  // ESC_IO_HPP
