#include <esc/detail/signals.hpp>

#include <csignal>
#include <cstdlib>
#include <stdexcept>

#include <esc/terminal.hpp>

namespace {

#if defined(__APPLE__) && defined(__MACH__)
#    define PLATFORM_EXIT() std::_Exit(1)
#else
#    define PLATFORM_EXIT() std::quick_exit(1)
#endif

extern "C" auto sigint_handler(int sig) -> void
{
    if (sig == SIGINT) {
        if (esc::sigint_flag == 1) {  // Force exit on second ctrl + c.
            PLATFORM_EXIT();
        }
        else {
            esc::sigint_flag = 1;
        }
    }
}

/**
 * Set the window_resize_sig flag to true on SIGWINCH signals.
 */
extern "C" auto resize_handler(int sig) -> void
{
    if (sig == SIGWINCH) {
        esc::detail::window_resize_sig = 1;
    }
}

}  // namespace

namespace esc {

std::sig_atomic_t sigint_flag = 0;

}  // namespace esc

namespace esc::detail {

std::sig_atomic_t window_resize_sig = 0;

auto register_signals(bool sigint) -> void
{
#if !defined(__APPLE__) && !defined(__MACH__)
    std::at_quick_exit(::esc::uninitialize_terminal);
#endif

    if (std::signal(SIGWINCH, &resize_handler) == SIG_ERR) {
        throw std::runtime_error{"register_SIGWINCH(): std::signal call"};
    }

    if (sigint && std::signal(SIGINT, &sigint_handler) == SIG_ERR) {
        throw std::runtime_error{"register_SIGINT(): std::signal call"};
    }
}

}  // namespace esc::detail
