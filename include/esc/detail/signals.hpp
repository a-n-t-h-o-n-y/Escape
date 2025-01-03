#pragma once

#include <csignal>

namespace esc {

/**
 * SIGINT flag set to true when SIGINT is received.
 * @details This should be periodically checked by your event loop.
 */
extern std::sig_atomic_t sigint_flag;

}  // namespace esc

namespace esc::detail {

/**
 * Notifies read() and next_state(Initial) that the window has been resized.
 * @details Used by SIGWINCH handler and accessed in esc/io.cpp.
 */
extern std::sig_atomic_t window_resize_sig;

/**
 * Install signal handlers for SIGWINCH and SIGINT, if sigint is true.
 * @param sigint If true, install a signal handler for SIGINT.
 * @throws std::runtime_error If a signal handler cannot be installed.
 */
void register_signals(bool sigint);

}  // namespace esc::detail