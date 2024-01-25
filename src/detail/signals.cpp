#include <esc/detail/signals.hpp>

#include <csignal>
#include <cstdlib>
#include <stdexcept>

#include <esc/terminal.hpp>

namespace {

extern "C" auto sigint_handler(int sig) -> void
{
    if (sig == SIGINT) {
        if (esc::sigint_flag == 1) {
            // If this is second time ctrl + c is pressed, force exit.
#if defined(__APPLE__) && defined(__MACH__)
            std::_Exit(1);
#else
            std::quick_exit(1);
#endif
        }
        else {
            esc::sigint_flag = 1;
        }
    }
}

/// Set the window_resize_sig flag to true on SIGWINCH signals.
extern "C" void resize_handler(int sig)
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

void register_signals(bool sigint)
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
