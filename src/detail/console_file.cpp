#include <esc/detail/console_file.hpp>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <array>
#include <stdexcept>

namespace {

// from linux/kd.h
constexpr auto get_kb_type = 0x4B33;
constexpr auto get_kb_mode = 0x4B44;
constexpr auto set_kb_mode = 0x4B45;
constexpr auto kb_84       = 0x01;
constexpr auto kb_101      = 0x02;

constexpr auto console_paths = std::array<char const*, 6>{
    "/proc/self/fd/0", "/dev/tty",    "/dev/tty0",
    "/dev/vc/0",       "/dev/systty", "/dev/console"};

/// Return true if \p file_descriptor is a console.
[[nodiscard]] auto is_a_console(int file_descriptor) -> bool
{
    auto kb_type = '\0';
    return (::isatty(file_descriptor) &&
            ::ioctl(file_descriptor, get_kb_type, &kb_type) == 0) &&
           ((kb_type == kb_101) || (kb_type == kb_84));
}

/// Attempt to open a console \p filename.
/** Returns -1 for error, or the file descriptor if successful. */
[[nodiscard]] auto open_a_console(const char* filename) -> int
{
    auto fd = ::open(filename, O_RDWR);
    if (fd < 0)
        fd = ::open(filename, O_WRONLY);
    if (fd < 0)
        fd = ::open(filename, O_RDONLY);
    if (fd < 0)
        return -1;
    return fd;
}

}  // namespace

namespace esc::detail {

auto open_console_file() noexcept(false) -> int
{
    for (char const* path : ::console_paths) {
        auto fd = ::open_a_console(path);
        if (fd >= 0) {
            if (::is_a_console(fd))
                return fd;
            close(fd);
        }
    }

    for (auto fd = 0; fd < 3; ++fd) {
        if (::is_a_console(fd))
            return fd;
    }

    throw std::runtime_error{
        "open_console_file(): Can't open console file. Try sudo."};
}

auto current_keyboard_mode(int file_descriptor) noexcept(false) -> long
{
    auto mode = long{0};
    if (::ioctl(file_descriptor, get_kb_mode, &mode) == -1)
        throw std::runtime_error("current_keyboard_mode(): Failed.");
    return mode;
}

void set_keyboard_mode(int file_descriptor, long mode) noexcept(false)
{
    if (::ioctl(file_descriptor, set_kb_mode, mode) == -1)
        throw std::runtime_error{"set_keyboard_mode(): Failed."};
}

}  // namespace esc::detail
