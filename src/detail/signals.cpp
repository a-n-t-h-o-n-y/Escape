#include <esc/detail/signals.hpp>

#include <csignal>
#include <cstdlib>
#include <stdexcept>

#include <esc/terminal.hpp>

namespace {

extern "C" auto sigint_handler(int sig) -> void
{
    if (sig == SIGINT)
        std::quick_exit(EXIT_SUCCESS);
}

/// Set the window_resize_sig flag to true on SIGWINCH signals.
extern "C" void resize_handler(int sig)
{
    if (sig == SIGWINCH)
        esc::detail::window_resize_sig = true;
}

}  // namespace

namespace esc::detail {

bool window_resize_sig = false;

void register_signals(bool sigint)
{
    std::at_quick_exit(esc::uninitialize_terminal);

    if (std::signal(SIGWINCH, &resize_handler) == SIG_ERR)
        throw std::runtime_error{"register_SIGWINCH(): std::signal call"};

    if (sigint && std::signal(SIGINT, &sigint_handler) == SIG_ERR)
        throw std::runtime_error{"register_SIGINT(): std::signal call"};
}

}  // namespace esc::detail
