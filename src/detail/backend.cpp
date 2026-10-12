#include "backend.hpp"
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <cerrno>
#include <climits>
#include <system_error>
namespace esc::detail {
auto descriptor(std::FILE* stream) -> int { return stream ? ::fileno(stream) : -1; }
auto is_terminal(int fd) -> bool { return fd >= 0 && ::isatty(fd); }
Input::Input(int fd) : descriptor{fd}, terminal{is_terminal(fd)} {}
auto terminal_identity(int input, int output) -> std::uint64_t
{
    if (!is_terminal(input) || !is_terminal(output))
        throw std::system_error{
            std::make_error_code(std::errc::inappropriate_io_control_operation),
            "Terminal: requires terminal streams"};
    struct stat in{}, out{};
    if (::fstat(input, &in) < 0 || ::fstat(output, &out) < 0)
        throw std::system_error{errno, std::generic_category(),
                                "Terminal: device identity"};
    if (in.st_rdev != out.st_rdev)
        throw std::system_error{std::make_error_code(std::errc::invalid_argument),
                                "Terminal: streams refer to different devices"};
    return static_cast<std::uint64_t>(in.st_rdev);
}
auto transport_setup_required() noexcept -> bool { return false; }
void begin_session(Attributes&) {}
auto session_profile(int, int) -> ProfileReport
{ return resolve_profile(environment(), true); }
auto desired_attributes(Attributes captured, TerminalOptions const& options)
    -> Attributes
{
    if (options.echo) {
        if (*options.echo == Echo::On)
            captured.c_lflag |= ECHO;
        else
            captured.c_lflag &= ~(ECHO | ECHONL);
    }
    if (options.input_buffer) {
        if (*options.input_buffer == InputBuffer::Canonical)
            captured.c_lflag |= ICANON;
        else {
            captured.c_lflag &= ~(ICANON | IEXTEN);
            captured.c_iflag &= ~(ICRNL | INLCR | IGNCR | ISTRIP | IXON | IXOFF);
            captured.c_cc[VMIN] = 0;
            captured.c_cc[VTIME] = 0;
        }
    }
    if (options.signals) {
        if (*options.signals == Signals::On)
            captured.c_lflag |= ISIG;
        else {
            captured.c_lflag &= ~(ISIG | IEXTEN);
            captured.c_iflag &= ~(IXON | IXOFF);
        }
    }
    return captured;
}
auto read_bytes(Input& input, void* buffer, std::size_t count) -> std::ptrdiff_t
{
    auto [fail, limit] = ESC_INTERCEPT(Read, count);
    if (fail) return -1;
    return ::read(input.descriptor, buffer, limit);
}
auto poll_input(Input& input, int timeout) -> int
{
    auto [fail, limit] = ESC_INTERCEPT(Poll, 1);
    (void)limit;
    if (fail) return -1;
    pollfd descriptor{input.descriptor, POLLIN, 0};
    auto result = ::poll(&descriptor, 1, timeout);
    input.hung_up = descriptor.revents & POLLHUP;
    if (descriptor.revents & POLLNVAL) {
        errno = EBADF;
        return -1;
    }
    return result;
}
auto get_attributes(int fd, int output, Attributes* settings) -> int
{
    auto [fail, limit] = ESC_INTERCEPT(GetAttributes, 0);
    (void)limit;
    if (fail) return -1;
    (void)output;
    return ::tcgetattr(fd, settings);
}
auto set_attributes(int fd, Attributes const* settings) -> int
{
    auto [fail, limit] = ESC_INTERCEPT(SetAttributes, 0);
    (void)limit;
    if (fail) return -1;
    if (::tcsetattr(fd, TCSANOW, settings) < 0) return -1;
#if defined(__APPLE__)
    // XNU sets PENDIN when re-entering ICANON and preserves it across tcsetattr.
    // FIONREAD completes that pending reprocessing without consuming or flushing
    // input. Honor a captured PENDIN bit rather than clearing it deliberately.
    // https://github.com/apple-oss-distributions/xnu/blob/main/bsd/kern/tty.c
    if ((settings->c_lflag & ICANON) && !(settings->c_lflag & PENDIN)) {
        int available = 0;
        if (::ioctl(fd, FIONREAD, &available) < 0) return -1;
    }
#endif
    return 0;
}
auto get_size(int fd) -> Area
{
    auto [fail, limit] = ESC_INTERCEPT(GetSize, 0);
    (void)limit;
    struct winsize size{};
    if (fail || ::ioctl(fd, TIOCGWINSZ, &size) < 0)
        throw std::system_error{errno, std::generic_category(), "Terminal: query size"};
    return {size.ws_col, size.ws_row};
}
}  // namespace esc::detail
