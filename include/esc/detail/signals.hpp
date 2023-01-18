#ifndef ESC_DETAIL_SIGNALS
#define ESC_DETAIL_SIGNALS

namespace esc::detail {

/// Notifies read() and next_state(Initial) that the window has been resized.
/** Used by SIGWINCH handler and accessed in esc/io.cpp. */
extern bool window_resize_sig;

/// Install signal handlers for SIGWINCH and SIGINT, if sigint is true.
void register_signals(bool sigint);

}  // namespace esc::detail
#endif  // ESC_DETAIL_SIGNALS
