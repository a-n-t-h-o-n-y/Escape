#pragma once

#include <atomic>

namespace esc {

/// This is set to true when SIGINT is received. This should be periodically
/// checked by your event loop.
extern std::atomic<bool> sigint_flag;

}  // namespace esc

namespace esc::detail {

/// Notifies read() and next_state(Initial) that the window has been resized.
/** Used by SIGWINCH handler and accessed in esc/io.cpp. */
extern std::atomic<bool> window_resize_sig;

/// Install signal handlers for SIGWINCH and SIGINT, if sigint is true.
void register_signals(bool sigint);

}  // namespace esc::detail