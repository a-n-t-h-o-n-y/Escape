#include <fcntl.h>
#include <io.h>
#include <windows.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <charconv>
#include <cstdlib>
#include <cstring>
#include <esc/text.hpp>
#include <system_error>
#include "backend.hpp"

namespace esc::detail {
namespace {
auto handle(int fd) -> HANDLE
{
    return fd < 0 ? INVALID_HANDLE_VALUE
                  : reinterpret_cast<HANDLE>(::_get_osfhandle(fd));
}
auto failed() -> int
{
    switch (::GetLastError()) {
        case ERROR_INVALID_HANDLE: errno = EBADF; break;
        case ERROR_ACCESS_DENIED: errno = EACCES; break;
        case ERROR_NOT_ENOUGH_MEMORY: errno = ENOMEM; break;
        case ERROR_INVALID_PARAMETER: errno = EINVAL; break;
        case ERROR_OPERATION_ABORTED: errno = EINTR; break;
        default: errno = EIO; break;
    }
    return -1;
}
void check(BOOL success, char const* message)
{
    if (!success)
        throw std::system_error{static_cast<int>(::GetLastError()),
                                std::system_category(), message};
}
auto key_bytes(KEY_EVENT_RECORD const& key, Input& input) -> std::string
{
    if (!key.bKeyDown) return {};
    auto control = key.dwControlKeyState;
    auto modifier = 1u + ((control & SHIFT_PRESSED) ? 1u : 0u) +
                    ((control & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) ? 2u : 0u) +
                    ((control & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) ? 4u : 0u);
    char final = 0;
    unsigned number = 0;
    switch (key.wVirtualKeyCode) {
        case VK_UP: final = 'A'; break;
        case VK_DOWN: final = 'B'; break;
        case VK_RIGHT: final = 'C'; break;
        case VK_LEFT: final = 'D'; break;
        case VK_HOME: final = 'H'; break;
        case VK_END: final = 'F'; break;
        case VK_INSERT: number = 2; break;
        case VK_DELETE: number = 3; break;
        case VK_PRIOR: number = 5; break;
        case VK_NEXT: number = 6; break;
        default: break;
    }
    constexpr unsigned functions[] = {11, 12, 13, 14, 15, 17, 18, 19, 20, 21, 23, 24,
                                      25, 26, 28, 29, 31, 32, 33, 34, 42, 43, 44, 45};
    if (key.wVirtualKeyCode >= VK_F1 && key.wVirtualKeyCode <= VK_F24)
        number = functions[key.wVirtualKeyCode - VK_F1];
    auto replacement = [&]() -> std::string {
        if (!input.high_surrogate) return {};
        input.high_surrogate = 0;
        return "\xef\xbf\xbd";
    };
    if (number)
        return replacement() + "\x1b[" + std::to_string(number) +
               (modifier == 1 ? "" : ";" + std::to_string(modifier)) + "~";
    if (final)
        return replacement() + "\x1b[" +
               (modifier == 1 ? std::string{} : "1;" + std::to_string(modifier)) +
               final;
    if (key.wVirtualKeyCode == VK_TAB && (control & SHIFT_PRESSED))
        return replacement() + "\x1b[Z";
    char32_t scalar = key.uChar.UnicodeChar;
    if (!scalar) return {};
    if (scalar >= 0xd800 && scalar <= 0xdbff) {
        auto invalid = replacement();
        input.high_surrogate = static_cast<char16_t>(scalar);
        return invalid;
    }
    if (scalar >= 0xdc00 && scalar <= 0xdfff) {
        if (!input.high_surrogate) return "\xef\xbf\xbd";
        scalar = 0x10000 + ((input.high_surrogate - 0xd800) << 10) + scalar - 0xdc00;
    }
    std::string bytes;
    if (input.high_surrogate && scalar < 0x10000) bytes = "\xef\xbf\xbd";
    input.high_surrogate = 0;
    // Right Alt + Ctrl is AltGr; its printable character must not gain Escape.
    if ((control & LEFT_ALT_PRESSED) ||
        ((control & RIGHT_ALT_PRESSED) && !(control & LEFT_CTRL_PRESSED)))
        bytes += '\x1b';
    auto encoded = encode_utf8(scalar);
    bytes.append(encoded.view());
    return bytes;
}
void mouse_bytes(Input& input, MOUSE_EVENT_RECORD const& mouse)
{
    auto held = mouse.dwButtonState & 0x1f;
    auto previous = input.buttons;
    input.buttons = held;
    if (input.protocols.mouse == MouseProtocol::None ||
        input.mouse_mode == MouseMode::Off)
        return;
    unsigned modifiers =
        ((mouse.dwControlKeyState & SHIFT_PRESSED) ? 4u : 0u) |
        ((mouse.dwControlKeyState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) ? 8u : 0u) |
        ((mouse.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) ? 16u
                                                                              : 0u);
    auto append = [&](unsigned code, bool release = false) {
        auto at = mouse.dwMousePosition;
        if (input.output_descriptor >= 0) {
            CONSOLE_SCREEN_BUFFER_INFO viewport{};
            check(::GetConsoleScreenBufferInfo(handle(input.output_descriptor),
                                               &viewport),
                  "Reader: mouse viewport");
            at.X -= viewport.srWindow.Left;
            at.Y -= viewport.srWindow.Top;
        }
        if (at.X < 0 || at.Y < 0) return;
        auto x = static_cast<unsigned>(at.X) + 1;
        auto y = static_cast<unsigned>(at.Y) + 1;
        code |= modifiers;
        auto number = [&](unsigned value) {
            std::array<char, 16> bytes;
            auto result =
                std::to_chars(bytes.data(), bytes.data() + bytes.size(), value);
            input.pending.append(bytes.data(), result.ptr);
        };
        if (input.protocols.mouse == MouseProtocol::Sgr) {
            input.pending += "\x1b[<";
            number(code);
            input.pending += ';';
            number(x);
            input.pending += ';';
            number(y);
            input.pending += release ? 'm' : 'M';
        }
        else if (input.protocols.mouse == MouseProtocol::Urxvt) {
            input.pending += "\x1b[";
            number((release ? 3u | modifiers : code) + 32);
            input.pending += ';';
            number(x);
            input.pending += ';';
            number(y);
            input.pending += 'M';
        }
        else if (x <= 223 && y <= 223) {
            input.pending += "\x1b[M";
            input.pending += static_cast<char>((release ? 3u | modifiers : code) + 32);
            input.pending += static_cast<char>(x + 32);
            input.pending += static_cast<char>(y + 32);
        }
    };
    if (mouse.dwEventFlags & (MOUSE_WHEELED | MOUSE_HWHEELED)) {
        auto delta = static_cast<short>(HIWORD(mouse.dwButtonState));
        auto code = mouse.dwEventFlags & MOUSE_HWHEELED ? (delta > 0 ? 67u : 66u)
                                                        : (delta > 0 ? 64u : 65u);
        auto steps = std::max(1, std::abs(static_cast<int>(delta)) / WHEEL_DELTA);
        if (delta)
            for (int i = 0; i < steps; ++i)
                append(code);
    }
    else if (mouse.dwEventFlags & MOUSE_MOVED) {
        if (input.mouse_mode == MouseMode::Basic ||
            (input.mouse_mode == MouseMode::Drag && !held))
            return;
        auto code = held & 1    ? 0u
                    : held & 4  ? 1u
                    : held & 2  ? 2u
                    : held & 8  ? 128u
                    : held & 16 ? 129u
                                : 3u;
        append(code | 32);
    }
    else {
        constexpr unsigned masks[] = {1, 4, 2, 8, 16};
        constexpr unsigned codes[] = {0, 1, 2, 128, 129};
        for (unsigned i = 0; i < 5; ++i)
            if ((held ^ previous) & masks[i]) append(codes[i], !(held & masks[i]));
    }
}
void echo(Input const& input, std::string_view bytes)
{
    auto text = utf8_to_utf32(bytes);
    std::wstring wide;
    for (auto scalar : text) {
        if (scalar <= 0xffff)
            wide += static_cast<wchar_t>(scalar);
        else {
            wide += static_cast<wchar_t>(0xd800 + ((scalar - 0x10000) >> 10));
            wide += static_cast<wchar_t>(0xdc00 + ((scalar - 0x10000) & 1023));
        }
    }
    bool borrowed = input.output_descriptor >= 0;
    auto output = borrowed ? handle(input.output_descriptor)
                           : ::CreateFileW(L"CONOUT$", GENERIC_WRITE,
                                           FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                           OPEN_EXISTING, 0, nullptr);
    if (output == INVALID_HANDLE_VALUE) check(FALSE, "Reader: open echo output");
    DWORD error = 0;
    std::size_t offset = 0;
    while (offset < wide.size()) {
        DWORD written = 0;
        if (!::WriteConsoleW(output, wide.data() + offset,
                             static_cast<DWORD>(wide.size() - offset), &written,
                             nullptr)) {
            error = ::GetLastError();
            break;
        }
        if (!written) {
            error = ERROR_WRITE_FAULT;
            break;
        }
        offset += written;
    }
    if (!borrowed) ::CloseHandle(output);
    if (error)
        throw std::system_error{static_cast<int>(error), std::system_category(),
                                "Reader: echo"};
}
void accept(Input& input, std::string const& bytes, DWORD mode)
{
    if (bytes.empty()) return;
    if (!(mode & ENABLE_LINE_INPUT)) {
        input.pending += bytes;
        return;
    }
    if (bytes == "\b" || bytes == "\x7f") {
        if (!input.line.empty()) {
            auto last = input.line.size() - 1;
            while (last &&
                   (static_cast<unsigned char>(input.line[last]) & 0xc0) == 0x80)
                --last;
            input.line.resize(last);
            if (mode & ENABLE_ECHO_INPUT) echo(input, "\b \b");
        }
        return;
    }
    // Bound canonical editing storage independently of a caller's deadline.
    if (input.line.size() + bytes.size() > 4096)
        throw std::system_error{std::make_error_code(std::errc::value_too_large),
                                "Reader: console line too long"};
    input.line += bytes;
    if (mode & ENABLE_ECHO_INPUT) echo(input, bytes == "\r" ? "\r\n" : bytes);
    if (bytes == "\r" || bytes == "\n") {
        input.pending += input.line;
        input.line.clear();
    }
}
}  // namespace

auto descriptor(std::FILE* stream) -> int { return stream ? ::_fileno(stream) : -1; }
auto is_terminal(int fd) -> bool
{
    DWORD mode = 0;
    return ::GetConsoleMode(handle(fd), &mode) != FALSE;
}
// Optional console stream worker. Native-record input remains the default.
// One bounded ReadFile can remain pending across caller timeouts. Stop cancels
// and joins it before session modes or borrowed handles can be released.
struct Input::ConsoleRead {
    HANDLE source, ready = nullptr, command = nullptr, thread = nullptr;
    std::array<char, 4096> bytes{};
    DWORD count = 0, error = 0;
    std::size_t offset = 0;
    bool requested = false;
    std::atomic<bool> stopping{false}, done{false}, work{false};
    explicit ConsoleRead(HANDLE handle) : source{handle}
    {
        ready = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
        check(ready != nullptr, "Query: create read completion event");
        command = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!command) {
            auto error = ::GetLastError();
            ::CloseHandle(ready);
            ::SetLastError(error);
            check(FALSE, "Query: create read request event");
        }
    }
    static DWORD WINAPI run(void* context) noexcept
    {
        auto& read = *static_cast<ConsoleRead*>(context);
        while (true) {
            if (::WaitForSingleObject(read.command, INFINITE) == WAIT_FAILED) {
                read.error = ::GetLastError();
                read.done.store(true, std::memory_order_release);
                ::SetEvent(read.ready);
                return 0;
            }
            if (read.stopping.load(std::memory_order_acquire)) return 0;
            (void)read.work.exchange(false, std::memory_order_acquire);
            if (!::ReadFile(read.source, read.bytes.data(),
                            static_cast<DWORD>(read.bytes.size()), &read.count,
                            nullptr))
                read.error = ::GetLastError();
            read.done.store(true, std::memory_order_release);
            ::SetEvent(read.ready);
        }
    }
    void start()
    {
        count = error = 0;
        offset = 0;
        done.store(false, std::memory_order_relaxed);
        ::ResetEvent(ready);
        if (!thread) {
            thread = ::CreateThread(nullptr, 0, run, this, 0, nullptr);
            check(thread != nullptr, "Query: start console read");
        }
        requested = true;
        work.store(true, std::memory_order_release);
        ::SetEvent(command);
    }
    ~ConsoleRead() noexcept
    {
        stopping.store(true, std::memory_order_release);
        ::SetEvent(command);
        // Retrying closes the race where cancellation precedes ReadFile entry.
        while (thread && ::WaitForSingleObject(thread, 0) == WAIT_TIMEOUT) {
            ::CancelSynchronousIo(thread);
            ::WaitForSingleObject(thread, 1);
        }
        if (thread) ::CloseHandle(thread);
        ::CloseHandle(command);
        ::CloseHandle(ready);
    }
};
Input::~Input() = default;
void prepare_queries(Input& input)
{
    if (input.high_surrogate)
        throw std::logic_error{
            "Query: complete pending native UTF-16 input before switching transport"};
    if (!input.line.empty()) {
        input.pending += input.line;
        input.line.clear();
    }
    if (input.terminal && !input.console_read)
        input.console_read =
            std::make_unique<Input::ConsoleRead>(handle(input.descriptor));
}
void stop_queries(Input& input) noexcept { input.console_read.reset(); }
Input::Input(int fd) : descriptor{fd}, terminal{is_terminal(fd)}
{
    if (terminal) {
        pending.reserve(4096);
        line.reserve(4096);
    }
}
auto terminal_identity(int input, int output) -> std::uint64_t
{
    DWORD mode = 0;
    CONSOLE_SCREEN_BUFFER_INFO info{};
    DWORD events = 0;
    if (!::GetConsoleMode(handle(input), &mode) ||
        !::GetNumberOfConsoleInputEvents(handle(input), &events) ||
        !::GetConsoleScreenBufferInfo(handle(output), &info))
        throw std::system_error{
            std::make_error_code(std::errc::inappropriate_io_control_operation),
            "Terminal: requires console input and output"};
    // Windows attaches a process to at most one console. Screen buffers share
    // its input modes and code pages, so even distinct output buffers overlap.
    return 1;
}
auto query_attributes(Attributes captured) -> Attributes
{
    if (captured.input_mode & (ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT))
        throw std::invalid_argument{"Query: immediate input with echo off is required"};
    captured.input_mode |= ENABLE_VIRTUAL_TERMINAL_INPUT;
    captured.input_codepage = CP_UTF8;
    return captured;
}
auto transport_setup_required() noexcept -> bool { return true; }
void begin_session(Attributes& captured)
{
    auto previous = ::_setmode(captured.output, _O_BINARY);
    if (previous < 0)
        throw std::system_error{errno, std::generic_category(),
                                "Terminal: output binary mode"};
    captured.output_crt_mode = previous;
}
auto session_profile(int, int output) -> ProfileReport
{
    auto env = environment();
    TerminalProfile profile;
    profile.control = true;
    // The Windows console VT baseline approximates extended colors to its
    // sixteen-color table. A richer emulator can use an explicit profile.
    profile.colors = env.no_color ? ColorSupport::None : ColorSupport::Basic16;
    CONSOLE_SCREEN_BUFFER_INFOEX info{};
    info.cbSize = sizeof(info);
    if (::GetConsoleScreenBufferInfoEx(handle(output), &info)) {
        constexpr unsigned slots[] = {0, 4,  2,  6,  1, 5,  3,  7,
                                      8, 12, 10, 14, 9, 13, 11, 15};
        for (unsigned i = 0; i < 16; ++i) {
            auto color = info.ColorTable[slots[i]];
            profile.system_palette[i] = RGB{static_cast<std::uint32_t>(
                (GetRValue(color) << 16) | (GetGValue(color) << 8) | GetBValue(color))};
        }
    }
    profile.traits = Trait::Bold | Trait::Underline | Trait::Inverse;
    profile.alternate_screen = profile.cursor_visibility = true;
    ProfileReport result{profile, {}};
    constexpr Feature features[] = {
        Feature::Control,         Feature::Colors,           Feature::Traits,
        Feature::AlternateScreen, Feature::CursorVisibility, Feature::Mouse,
        Feature::Focus,           Feature::BracketedPaste};
    for (auto feature : features)
        result.decisions.push_back(
            {feature, EvidenceSource::KnownTerminal,
             "Conservative Windows console VT baseline; open checks VT mode"});
    if (env.no_color)
        result.decisions[1] = {Feature::Colors, EvidenceSource::Environment,
                               "NO_COLOR disables colors"};
    return result;
}
auto get_attributes(int input, int output, Attributes* settings) -> int
{
    auto [fail, limit] = ESC_INTERCEPT(GetAttributes, 0);
    if (fail) return -1;
    (void)limit;

    settings->output = output;
    if (!::GetConsoleMode(handle(input), &settings->input_mode) ||
        !::GetConsoleMode(handle(output), &settings->output_mode))
        return failed();
    settings->input_codepage = ::GetConsoleCP();
    settings->output_codepage = ::GetConsoleOutputCP();
    if (!settings->input_codepage || !settings->output_codepage) return failed();
    return 0;
}
auto desired_attributes(Attributes captured, TerminalOptions const& options)
    -> Attributes
{
    captured.output_mode |=
        ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    captured.output_codepage = CP_UTF8;
    captured.output_crt_mode = _O_BINARY;
    // Native records give deadline-safe Unicode input without waiting for a
    // ReadConsole line, and retain key repeats and modifiers.
    captured.input_mode &= ~ENABLE_VIRTUAL_TERMINAL_INPUT;
    captured.input_mode |= ENABLE_EXTENDED_FLAGS;
    captured.input_mode &= ~ENABLE_QUICK_EDIT_MODE;
    if (options.mouse.value_or(options.restore_modes.mouse.mode) != MouseMode::Off)
        captured.input_mode |= ENABLE_MOUSE_INPUT;
    else
        captured.input_mode &= ~ENABLE_MOUSE_INPUT;
    if (options.echo) {
        if (*options.echo == Echo::On)
            captured.input_mode |= ENABLE_ECHO_INPUT;
        else
            captured.input_mode &= ~ENABLE_ECHO_INPUT;
    }
    if (options.input_buffer) {
        if (*options.input_buffer == InputBuffer::Canonical)
            captured.input_mode |= ENABLE_LINE_INPUT;
        else
            captured.input_mode &= ~ENABLE_LINE_INPUT;
    }
    if (options.signals) {
        if (*options.signals == Signals::On)
            captured.input_mode |= ENABLE_PROCESSED_INPUT;
        else
            captured.input_mode &= ~ENABLE_PROCESSED_INPUT;
    }
    // Windows forbids echo without line input; reject before changing modes.
    if ((captured.input_mode & ENABLE_ECHO_INPUT) &&
        !(captured.input_mode & ENABLE_LINE_INPUT))
        throw std::invalid_argument{"Terminal: Windows echo requires canonical input"};
    return captured;
}
auto set_attributes(int input, Attributes const* settings) -> int
{
    auto [fail, limit] = ESC_INTERCEPT(SetAttributes, 0);
    if (fail) return -1;
    (void)limit;

    // Attempt every component, including restoration after a partial failure.
    DWORD error = 0;
    if (!::SetConsoleMode(handle(input), settings->input_mode | ENABLE_EXTENDED_FLAGS))
        error = ::GetLastError();
    if (!(settings->input_mode & ENABLE_EXTENDED_FLAGS) &&
        !::SetConsoleMode(handle(input), settings->input_mode) && !error)
        error = ::GetLastError();
    if (!::SetConsoleMode(handle(settings->output), settings->output_mode) && !error)
        error = ::GetLastError();
    if (!::SetConsoleCP(settings->input_codepage) && !error) error = ::GetLastError();
    if (!::SetConsoleOutputCP(settings->output_codepage) && !error)
        error = ::GetLastError();
    if (settings->output_crt_mode != -1 &&
        ::_setmode(settings->output, settings->output_crt_mode) < 0) {
        if (!error) return -1;
    }
    if (error) {
        ::SetLastError(error);
        return failed();
    }
    return 0;
}
auto get_size(int fd) -> Area
{
    auto [fail, limit] = ESC_INTERCEPT(GetSize, 0);
    if (fail)
        throw std::system_error{errno, std::generic_category(), "Terminal: query size"};
    (void)limit;

    CONSOLE_SCREEN_BUFFER_INFO info{};
    check(::GetConsoleScreenBufferInfo(handle(fd), &info), "Terminal: query viewport");
    return {info.srWindow.Right - info.srWindow.Left + 1,
            info.srWindow.Bottom - info.srWindow.Top + 1};
}
auto poll_input(Input& input, int timeout) -> int
{
    auto [fail, limit] = ESC_INTERCEPT(Poll, 1);
    if (fail) return -1;
    (void)limit;

    if (input.console_read && !(input.offset < input.pending.size() || input.repeat)) {
        auto& read = *input.console_read;
        if (read.done.load(std::memory_order_acquire) &&
            (read.offset < read.count || read.error))
            return 1;
        if (!read.requested) read.start();
        auto result = ::WaitForSingleObject(
            read.ready, timeout < 0 ? INFINITE : static_cast<DWORD>(timeout));
        if (result == WAIT_TIMEOUT) return 0;
        if (result == WAIT_FAILED) return failed();
        (void)read.done.load(std::memory_order_acquire);
        return 1;
    }

    auto until = timeout < 0
                     ? Clock::time_point::max()
                     : deadline_after(Clock::now(), std::chrono::milliseconds{timeout});
    auto source = handle(input.descriptor);
    if (source == INVALID_HANDLE_VALUE) {
        errno = EBADF;
        return -1;
    }
    auto kind = ::GetFileType(source);
    DWORD zero_poll_budget = 0;
    if (input.terminal && timeout == 0 &&
        !::GetNumberOfConsoleInputEvents(source, &zero_poll_budget))
        return failed();
    while (true) {
        if (input.offset < input.pending.size() || input.repeat) return 1;
        input.pending.clear();
        input.offset = 0;
        if (input.terminal) {
            DWORD mode = 0, available = 0;
            if (!::GetConsoleMode(source, &mode) ||
                !::GetNumberOfConsoleInputEvents(source, &available))
                return failed();
            if (!(mode & ENABLE_LINE_INPUT) && !input.line.empty()) {
                input.pending.swap(input.line);
                return 1;
            }
            if (available) {
                INPUT_RECORD record{};
                DWORD count = 0;
                if (!::ReadConsoleInputW(source, &record, 1, &count)) return failed();
                if (count && record.EventType == KEY_EVENT) {
                    auto bytes = key_bytes(record.Event.KeyEvent, input);
                    auto repetitions = record.Event.KeyEvent.wRepeatCount;
                    if (!(mode & ENABLE_LINE_INPUT) && !bytes.empty() && repetitions) {
                        input.repeated = bytes;
                        input.repeat = repetitions;
                        return 1;
                    }
                    for (unsigned i = 0; i < repetitions; ++i)
                        accept(input, bytes, mode);
                }
                if (count && record.EventType == MOUSE_EVENT)
                    mouse_bytes(input, record.Event.MouseEvent);
                if (count && record.EventType == FOCUS_EVENT && input.protocols.focus)
                    input.pending +=
                        record.Event.FocusEvent.bSetFocus ? "\x1b[I" : "\x1b[O";
                if (!input.pending.empty()) return 1;
                if (timeout == 0) {
                    if (!zero_poll_budget || --zero_poll_budget == 0) return 0;
                }
                else if (Clock::now() >= until)
                    return 0;
                continue;
            }
            auto delay = until == Clock::time_point::max()
                             ? INFINITE
                             : static_cast<DWORD>(poll_delay(until));
            auto result = ::WaitForSingleObject(source, delay);
            if (result == WAIT_FAILED) return failed();
            if (result == WAIT_TIMEOUT) return 0;
        }
        else if (kind == FILE_TYPE_PIPE) {
            DWORD available = 0;
            if (!::PeekNamedPipe(source, nullptr, 0, nullptr, &available, nullptr)) {
                if (::GetLastError() == ERROR_BROKEN_PIPE ||
                    ::GetLastError() == ERROR_PIPE_NOT_CONNECTED) {
                    input.hung_up = true;
                    return 1;
                }
                return failed();
            }
            if (available) return 1;
            if (Clock::now() >= until) return 0;
            // Synchronous anonymous pipes expose no waitable readiness event.
            // A bounded sleep avoids spinning while respecting caller deadlines.
            ::Sleep(static_cast<DWORD>(std::min(2, poll_delay(until))));
        }
        else if (kind == FILE_TYPE_DISK)
            return 1;
        else {
            errno = ENOTSUP;
            return -1;
        }
    }
}
auto read_bytes(Input& input, void* buffer, std::size_t count) -> std::ptrdiff_t
{
    auto [fail, limit] = ESC_INTERCEPT(Read, count);
    if (fail) return -1;
    count = limit;

    if (input.console_read && !(input.offset < input.pending.size() || input.repeat)) {
        auto& read = *input.console_read;
        if (!read.done.load(std::memory_order_acquire)) {
            errno = EAGAIN;
            return -1;
        }
        if (read.error) {
            ::SetLastError(read.error);
            return failed();
        }
        auto size = std::min(count, static_cast<std::size_t>(read.count) - read.offset);
        std::memcpy(buffer, read.bytes.data() + read.offset, size);
        read.offset += size;
        if (read.offset == read.count) {
            read.done.store(false, std::memory_order_release);
            read.requested = false;
        }
        if (!size) input.hung_up = true;
        return static_cast<std::ptrdiff_t>(size);
    }
    if (input.terminal) {
        if (input.offset == input.pending.size() && input.repeat) {
            input.pending = input.repeated;
            input.offset = 0;
            --input.repeat;
        }
        auto size = std::min(count, input.pending.size() - input.offset);
        std::memcpy(buffer, input.pending.data() + input.offset, size);
        input.offset += size;
        return static_cast<std::ptrdiff_t>(size);
    }
    DWORD available = 0;
    auto source = handle(input.descriptor);
    if (input.hung_up) return 0;
    if (::GetFileType(source) == FILE_TYPE_PIPE) {
        if (!::PeekNamedPipe(source, nullptr, 0, nullptr, &available, nullptr)) {
            if (::GetLastError() == ERROR_BROKEN_PIPE) return 0;
            return failed();
        }
        count = std::min(count, static_cast<std::size_t>(available));
        if (!count) {
            errno = EAGAIN;
            return -1;
        }
    }
    DWORD read = 0;
    if (!::ReadFile(source, buffer,
                    static_cast<DWORD>(std::min<std::size_t>(count, MAXDWORD)), &read,
                    nullptr)) {
        if (::GetLastError() == ERROR_BROKEN_PIPE ||
            ::GetLastError() == ERROR_HANDLE_EOF)
            return 0;
        return failed();
    }
    return read;
}
}  // namespace esc::detail
