#include <algorithm>
#include <chrono>
#include <climits>
#include <esc/esc.hpp>
#include <iostream>
#include <thread>
#ifdef _WIN32
#    define NOMINMAX
#    include <windows.h>
#else
#    include <poll.h>
#    include <cerrno>
#    include <system_error>
#endif

using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;

// Add host sockets or a shutdown source to this same wait operation.
void wait(esc::WaitState const& state, Clock::time_point application_deadline)
{
    auto until = state.deadline ? std::min(*state.deadline, application_deadline)
                                : application_deadline;
    auto remaining = until - Clock::now();
    if (remaining <= Clock::duration::zero()) return;
    auto milliseconds = std::chrono::ceil<std::chrono::milliseconds>(remaining).count();
#ifdef _WIN32
    if (state.source.kind == esc::WaitSourceKind::WindowsHandle) {
        auto handle = reinterpret_cast<HANDLE>(state.source.value);
        auto delay =
            static_cast<DWORD>(std::min<long long>(milliseconds, INFINITE - 1));
        if (WaitForMultipleObjects(1, &handle, FALSE, delay) == WAIT_FAILED)
            throw std::system_error{static_cast<int>(GetLastError()),
                                    std::system_category(), "event-loop wait"};
        return;
    }
#else
    if (state.source.kind == esc::WaitSourceKind::FileDescriptor) {
        pollfd source{static_cast<int>(state.source.value), POLLIN, 0};
        if (poll(&source, 1,
                 static_cast<int>(std::min<long long>(milliseconds, INT_MAX))) < 0 &&
            errno != EINTR)
            throw std::system_error{errno, std::generic_category(), "event-loop poll"};
        return;
    }
#endif
    // Windows synchronous pipes/files use Escape's short fallback deadline.
    std::this_thread::sleep_until(until);
}

int main()
{
    try {
        auto terminal = esc::Terminal::open(esc::TerminalOptions::interactive());
        auto tick = Clock::now();
        unsigned seconds = 0;
        for (;;) {
            if (Clock::now() >= tick) {
                auto frame = terminal.frame();
                terminal.write(esc::Cursor{}, esc::BlankRow{},
                               esc::styled("Press q to quit.",
                                           esc::bold | esc::fg(esc::colors::cyan)),
                               " Timer: ", std::to_string(seconds++));
                frame.close();
                tick = Clock::now() + 1s;
            }
            auto state = terminal.wait_state();
            if (!state.ready) wait(state, tick);
            auto result = terminal.read(0ms);
            if (std::holds_alternative<esc::EndOfInput>(result)) break;
            if (auto event = std::get_if<esc::Event>(&result)) {
                auto key = std::get_if<esc::KeyEvent>(event);
                if (key && key->matches(U'q')) break;
            }
        }
        terminal.close();
        return 0;
    }
    catch (std::exception const& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
