#ifndef ESC_IO_HPP
#define ESC_IO_HPP
#include <array>
#include <climits>
#include <clocale>
#include <cstddef>
#include <cstdio>
#include <cuchar>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include <esc/detail/is_convertible_to_any_of.hpp>
#include <esc/detail/is_urxvt.hpp>
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
    auto chars       = std::array<char, MB_LEN_MAX>{};
    auto state       = std::mbstate_t{};
    auto const count = std::c32rtomb(chars.data(), c, &state);
    if (count == std::size_t(-1))
        throw std::runtime_error{"Invalid char32_t to MB conversion."};
    for (auto i = std::size_t{0}; i < count; ++i)
        write(chars[i]);
}

/// Writes each char of \p sv to the console via stdout.
inline auto write(std::string_view sv) -> void
{
    for (auto c : sv)
        write(c);
}

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
                                     std::string_view,
                                     std::u32string_view>;

/// Writes any number of writable objects, in paramter order.
template <typename... Args>
void write(Args&&... args)
{
    static_assert(sizeof...(Args) > 0,
                  "write(...): Must have at least one argument.");
    static_assert((is_writable<Args> && ...),
                  "write(...): All Args... must be writable types.");

    auto sv_cast = [](auto w) {
        if constexpr (std::is_convertible_v<decltype(w), std::string_view>)
            return std::string_view{w};
        else if constexpr (std::is_convertible_v<decltype(w),
                                                 std::u32string_view>) {
            return std::u32string_view{w};
        }
        else
            return w;
    };
    (write(sv_cast(args)), ...);
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
