#include <fcntl.h>
#include <io.h>
#include <windows.h>
#include <array>
#include <chrono>
#include <cstdio>
#include <esc/esc.hpp>
#include <thread>
#include <utility>
#include "support/faults.hpp"
#include "support/test.hpp"

namespace {
using namespace std::chrono_literals;
struct Console {
    bool owned = false;
    Console()
    {
        // Keep console injection isolated from the invoking shell.
        ::FreeConsole();
        CHECK(::AllocConsole());
        owned = true;
    }
    ~Console()
    {
        if (owned) ::FreeConsole();
    }
};
auto console() -> Console&
{
    static Console result;
    return result;
}
struct File {
    std::FILE* stream = nullptr;
    explicit File(HANDLE handle, int flags)
    {
        CHECK(handle != INVALID_HANDLE_VALUE);
        auto fd =
            ::_open_osfhandle(reinterpret_cast<intptr_t>(handle), flags | _O_BINARY);
        if (fd < 0) {
            ::CloseHandle(handle);
            CHECK(false);
        }
        stream = ::_fdopen(fd, flags & _O_WRONLY ? "wb" : "rb");
        if (!stream) {
            ::_close(fd);
            CHECK(false);
        }
    }
    File(File const&) = delete;
    ~File()
    {
        if (stream) std::fclose(stream);
    }
    auto handle() const -> HANDLE
    { return reinterpret_cast<HANDLE>(::_get_osfhandle(::_fileno(stream))); }
};
struct Fixture {
    File input;
    File output;
    explicit Fixture(bool initialize = true)
        : input{(console(),
                 ::CreateFileW(L"CONIN$",
                               GENERIC_READ | GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE,
                               nullptr,
                               OPEN_EXISTING,
                               0,
                               nullptr)),
                _O_RDONLY},
          output{::CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE,
                                             FILE_SHARE_READ | FILE_SHARE_WRITE,
                                             nullptr,
                                             CONSOLE_TEXTMODE_BUFFER,
                                             nullptr),
                 _O_WRONLY}
    {
        if (!initialize) return;
        CHECK(::FlushConsoleInputBuffer(input.handle()));
        DWORD mode = 0;
        CHECK(::GetConsoleMode(input.handle(), &mode));
        CHECK(::SetConsoleMode(
            input.handle(), (mode | ENABLE_LINE_INPUT) &
                                ~(ENABLE_ECHO_INPUT | ENABLE_VIRTUAL_TERMINAL_INPUT)));
    }
    auto streams() -> esc::Streams { return {input.stream, output.stream}; }
    auto input_mode() const -> DWORD
    {
        DWORD mode = 0;
        CHECK(::GetConsoleMode(input.handle(), &mode));
        return mode;
    }
    auto output_mode() const -> DWORD
    {
        DWORD mode = 0;
        CHECK(::GetConsoleMode(output.handle(), &mode));
        return mode;
    }
    void record(INPUT_RECORD record)
    {
        DWORD written = 0;
        CHECK(::WriteConsoleInputW(input.handle(), &record, 1, &written));
        CHECK(written == 1);
    }
    void key(wchar_t scalar,
             WORD code = 0,
             DWORD modifiers = 0,
             WORD repeat = 1,
             bool down = true)
    {
        INPUT_RECORD event{};
        event.EventType = KEY_EVENT;
        event.Event.KeyEvent = {down, repeat, code, 0, {}, modifiers};
        event.Event.KeyEvent.uChar.UnicodeChar = scalar;
        record(event);
    }
};
auto key(esc::Reader& reader, esc::Key expected, esc::Modifiers modifiers = {}) -> void
{
    auto result = reader.read(100ms);
    CHECK(result.status == esc::ReadStatus::Event);
    CHECK(result.event ==
          std::optional<esc::Event>{esc::KeyPress{expected, modifiers}});
}
auto immediate(Fixture& fixture) -> esc::Terminal
{
    esc::TerminalOptions options;
    options.input_buffer = esc::InputBuffer::Immediate;
    options.echo = esc::Echo::Off;
    return esc::Terminal::open(fixture.streams(), options);
}
struct Pipe {
    File input;
    HANDLE output = INVALID_HANDLE_VALUE;
    static auto create(HANDLE& output) -> HANDLE
    {
        HANDLE input = nullptr;
        CHECK(::CreatePipe(&input, &output, nullptr, 0));
        return input;
    }
    // output must be initialized before create writes it.
    Pipe(HANDLE read, HANDLE write) : input{read, _O_RDONLY}, output{write} {}
    static auto make() -> Pipe
    {
        HANDLE write = nullptr;
        auto read = create(write);
        return Pipe{read, write};
    }
    ~Pipe() { close(); }
    void close()
    {
        if (output != INVALID_HANDLE_VALUE) {
            ::CloseHandle(output);
            output = INVALID_HANDLE_VALUE;
        }
    }
    void write(std::string_view bytes)
    {
        DWORD count = 0;
        CHECK(::WriteFile(output, bytes.data(), static_cast<DWORD>(bytes.size()),
                          &count, nullptr));
        CHECK(count == bytes.size());
    }
};
}  // namespace

TEST(WIN_SESSION_restores_input_output_modes_and_codepages)
{
    Fixture fixture;
    auto in = fixture.input_mode(), out = fixture.output_mode();
    auto input_cp = ::GetConsoleCP(), output_cp = ::GetConsoleOutputCP();
    {
        auto terminal = immediate(fixture);
        CHECK(!(fixture.input_mode() &
                (ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_QUICK_EDIT_MODE)));
        CHECK(fixture.output_mode() & ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        CHECK(::GetConsoleOutputCP() == CP_UTF8);
        CHECK(terminal.area().width > 0);
        terminal.close();
        terminal.close();
    }
    CHECK(fixture.input_mode() == in);
    CHECK(fixture.output_mode() == out);
    CHECK(::GetConsoleCP() == input_cp);
    CHECK(::GetConsoleOutputCP() == output_cp);
}
TEST(WIN_SESSION_move_transfers_cleanup)
{
    Fixture fixture;
    auto before = fixture.input_mode();
    {
        auto first = immediate(fixture);
        auto second = std::move(first);
        CHECK(!first.is_open());
        CHECK(second.is_open());
    }
    CHECK(fixture.input_mode() == before);
}
TEST(WIN_SESSION_shared_console_has_one_owner)
{
    Fixture fixture;
    auto first = immediate(fixture);
    auto active = fixture.input_mode();
    Fixture other{false};
    CHECK_THROWS(std::system_error, immediate(other));
    CHECK(fixture.input_mode() == active);
    first.close();
    auto next = immediate(other);
    CHECK(next.is_open());
}
TEST(WIN_SESSION_redirected_streams_rejected)
{
    auto pipe = Pipe::make();
    CHECK_THROWS(std::system_error,
                 esc::Terminal::open(esc::Streams{pipe.input.stream, stdout}));
}
TEST(WIN_SESSION_explicit_profile_does_not_override_drawing_limits)
{
    Fixture fixture;
    esc::TerminalOptions options;
    options.profile = esc::TerminalProfile{};
    auto terminal = esc::Terminal::open(fixture.streams(), options);
    CHECK(!terminal.profile().control);
    CHECK_THROWS(esc::UnsupportedFeature, terminal.send(esc::Cursor{}));
}
TEST(WIN_SESSION_conservative_default_rejects_unverified_protocols)
{
    Fixture fixture;
    auto terminal = immediate(fixture);
    CHECK_THROWS(esc::UnsupportedFeature, terminal.set(esc::MouseMode::Move));
    CHECK_THROWS(esc::UnsupportedFeature, terminal.set(esc::FocusReporting::On));
    CHECK_THROWS(esc::UnsupportedFeature, terminal.set(esc::BracketedPaste::On));
}
TEST(WIN_SESSION_failed_validation_does_not_change_modes)
{
    Fixture fixture;
    auto in = fixture.input_mode(), out = fixture.output_mode();
    esc::TerminalOptions options = esc::TerminalOptions::interactive();
    options.resize_interval = 0ms;
    CHECK_THROWS(std::invalid_argument,
                 esc::Terminal::open(fixture.streams(), options));
    CHECK(fixture.input_mode() == in);
    CHECK(fixture.output_mode() == out);
}
TEST(WIN_SESSION_signals_preserve_unrelated_flags)
{
    Fixture fixture;
    auto before = fixture.input_mode();
    auto terminal = immediate(fixture);
    terminal.set(esc::Signals::Off);
    CHECK(!(fixture.input_mode() & ENABLE_PROCESSED_INPUT));
    terminal.set(esc::Signals::On);
    CHECK(fixture.input_mode() & ENABLE_PROCESSED_INPUT);
    terminal.close();
    CHECK(fixture.input_mode() == before);
}
TEST(WIN_INPUT_unicode_surrogate_pair)
{
    Fixture fixture;
    auto terminal = immediate(fixture);
    esc::Reader reader{fixture.input.stream};
    fixture.key(0xd83d);
    fixture.key(0xdc7d);
    key(reader, char32_t{0x1f47d});
}
TEST(WIN_INPUT_altgr_is_text)
{
    Fixture fixture;
    auto terminal = immediate(fixture);
    esc::Reader reader{fixture.input.stream};
    fixture.key(L'€', 0, LEFT_CTRL_PRESSED | RIGHT_ALT_PRESSED);
    key(reader, U'€');
}
TEST(WIN_INPUT_alt_text)
{
    Fixture fixture;
    auto terminal = immediate(fixture);
    esc::Reader reader{fixture.input.stream};
    fixture.key(L'x', 0, LEFT_ALT_PRESSED);
    key(reader, U'x', {.alt = true});
}
TEST(WIN_INPUT_arrows_and_modifiers)
{
    Fixture fixture;
    auto terminal = immediate(fixture);
    esc::Reader reader{fixture.input.stream};
    fixture.key(0, VK_UP, SHIFT_PRESSED | LEFT_CTRL_PRESSED);
    key(reader, esc::KeyCode::ArrowUp, {.shift = true, .ctrl = true});
}
TEST(WIN_INPUT_function_keys)
{
    Fixture fixture;
    auto terminal = immediate(fixture);
    esc::Reader reader{fixture.input.stream};
    for (WORD code = VK_F1; code <= VK_F24; ++code) {
        fixture.key(0, code, SHIFT_PRESSED | LEFT_CTRL_PRESSED);
        key(reader,
            static_cast<esc::KeyCode>(static_cast<unsigned>(esc::KeyCode::F1) + code -
                                      VK_F1),
            {.shift = true, .ctrl = true});
    }
}
TEST(WIN_INPUT_canonical_echo_uses_session_buffer_without_leaking_handles)
{
    Fixture fixture;
    esc::TerminalOptions options;
    options.echo = esc::Echo::On;
    options.input_buffer = esc::InputBuffer::Canonical;
    options.profile = esc::TerminalProfile{.control = true};
    auto terminal = esc::Terminal::open(fixture.streams(), options);
    CHECK(::SetConsoleCursorPosition(fixture.output.handle(), {0, 0}));
    DWORD before = 0, after = 0;
    CHECK(::GetProcessHandleCount(::GetCurrentProcess(), &before));
    for (int i = 0; i < 32; ++i) {
        fixture.key(L'x');
        CHECK(terminal.read(0ms).status == esc::ReadStatus::Timeout);
    }
    CHECK(::GetProcessHandleCount(::GetCurrentProcess(), &after));
    CHECK(after == before);
    std::array<wchar_t, 32> text{};
    DWORD count = 0;
    CHECK(::ReadConsoleOutputCharacterW(fixture.output.handle(), text.data(),
                                        static_cast<DWORD>(text.size()), {0, 0},
                                        &count));
    CHECK(count == text.size());
    for (auto character : text)
        CHECK(character == L'x');
}
TEST(WIN_INPUT_repeats_do_not_require_more_os_records)
{
    Fixture fixture;
    auto terminal = immediate(fixture);
    esc::Reader reader{fixture.input.stream};
    fixture.key(L'x', 0, 0, 3);
    key(reader, U'x');
    key(reader, U'x');
    key(reader, U'x');
    CHECK(reader.read(0ms).status == esc::ReadStatus::Timeout);
}
TEST(WIN_INPUT_keyup_and_window_events_do_not_block)
{
    Fixture fixture;
    auto terminal = immediate(fixture);
    esc::Reader reader{fixture.input.stream};
    fixture.key(L'x', 0, 0, 1, false);
    INPUT_RECORD record{};
    record.EventType = WINDOW_BUFFER_SIZE_EVENT;
    fixture.record(record);
    auto start = std::chrono::steady_clock::now();
    CHECK(reader.read(10ms).status == esc::ReadStatus::Timeout);
    CHECK(std::chrono::steady_clock::now() - start < 500ms);
}
TEST(WIN_INPUT_canonical_waits_for_enter_and_edits_backspace)
{
    Fixture fixture;
    esc::Reader reader{fixture.input.stream};
    fixture.key(L'a');
    fixture.key(L'b');
    fixture.key(L'\b', VK_BACK);
    CHECK(reader.read(10ms).status == esc::ReadStatus::Timeout);
    fixture.key(L'\r', VK_RETURN);
    key(reader, U'a');
    key(reader, esc::KeyCode::Enter);
}
TEST(WIN_INPUT_pipe_timeout_then_data_then_eof)
{
    auto pipe = Pipe::make();
    esc::Reader reader{pipe.input.stream};
    CHECK(reader.read(5ms).status == esc::ReadStatus::Timeout);
    pipe.write("\x1b[A");
    key(reader, esc::KeyCode::ArrowUp);
    pipe.close();
    CHECK(reader.read(100ms).status == esc::ReadStatus::EndOfInput);
    CHECK(reader.read(0ms).status == esc::ReadStatus::EndOfInput);
}
TEST(WIN_INPUT_pipe_deadline_wakes_on_later_data)
{
    auto pipe = Pipe::make();
    esc::Reader reader{pipe.input.stream};
    std::jthread writer{[&] {
        std::this_thread::sleep_for(10ms);
        pipe.write("x");
    }};
    key(reader, U'x');
}
TEST(WIN_INPUT_pipe_eof_finishes_incomplete_utf8)
{
    auto pipe = Pipe::make();
    pipe.write("\xf0\x9f");
    pipe.close();
    esc::Reader reader{pipe.input.stream};
    auto result = reader.read(100ms);
    CHECK(result.status == esc::ReadStatus::Event);
    CHECK(std::get<esc::UnknownInput>(*result.event).problem ==
          esc::InputProblem::Truncated);
    CHECK(reader.read(0ms).status == esc::ReadStatus::EndOfInput);
}
TEST(WIN_OUTPUT_binary_file_preserves_bytes_and_unicode)
{
    auto* stream = std::tmpfile();
    CHECK(stream);
    CHECK(::_setmode(::_fileno(stream), _O_BINARY) >= 0);
    esc::Output output{stream};
    output.write(std::string_view{"a\0\n", 3}, U'界');
    output.flush();
    std::rewind(stream);
    std::array<char, 16> bytes{};
    auto count = std::fread(bytes.data(), 1, bytes.size(), stream);
    CHECK(count == 6);
    CHECK(std::string_view{bytes.data(), count} ==
          std::string_view{"a\0\n\xe7\x95\x8c", 6});
    CHECK(std::fclose(stream) == 0);
}
TEST(WIN_INPUT_enabled_focus_and_sgr_mouse)
{
    Fixture fixture;
    auto terminal = immediate(fixture);
    esc::Reader reader{
        fixture.input.stream, {}, {esc::MouseProtocol::Sgr, true, false}};
    INPUT_RECORD focus{};
    focus.EventType = FOCUS_EVENT;
    focus.Event.FocusEvent.bSetFocus = TRUE;
    fixture.record(focus);
    CHECK(reader.read(100ms).event == std::optional<esc::Event>{esc::Focus{true}});
    INPUT_RECORD mouse{};
    mouse.EventType = MOUSE_EVENT;
    mouse.Event.MouseEvent.dwMousePosition = {4, 2};
    mouse.Event.MouseEvent.dwButtonState = FROM_LEFT_1ST_BUTTON_PRESSED;
    fixture.record(mouse);
    CHECK(reader.read(100ms).event ==
          std::optional<esc::Event>{esc::MousePress{{4, 2}, esc::MouseButton::Left}});
    mouse.Event.MouseEvent.dwButtonState = 0;
    fixture.record(mouse);
    CHECK(reader.read(100ms).event ==
          std::optional<esc::Event>{esc::MouseRelease{{4, 2}, esc::MouseButton::Left}});
}

TEST(WIN_INPUT_zero_poll_drains_already_available_records)
{
    Fixture fixture;
    auto terminal = immediate(fixture);
    esc::Reader reader{fixture.input.stream};
    fixture.key(L'x', 0, 0, 1, false);
    fixture.key(L'z');
    CHECK(reader.read(0ms).event == std::optional<esc::Event>{esc::KeyPress{U'z'}});
}
TEST(WIN_INPUT_zero_poll_combines_available_surrogates)
{
    Fixture fixture;
    auto terminal = immediate(fixture);
    esc::Reader reader{fixture.input.stream};
    fixture.key(0xd83d);
    fixture.key(0xdc7d);
    CHECK(reader.read(0ms).event ==
          std::optional<esc::Event>{esc::KeyPress{char32_t{0x1f47d}}});
}

TEST(WIN_HEADERS_preserve_host_RGB_macro)
{
    constexpr esc::RGB color{1, 2, 3};
    CHECK(color.red == 1 && color.green == 2 && color.blue == 3);
    CHECK(RGB(1, 2, 3) == static_cast<COLORREF>(0x030201));
}
TEST(WIN_SESSION_restores_borrowed_CRT_output_mode)
{
    Fixture fixture;
    CHECK(::_setmode(::_fileno(fixture.output.stream), _O_TEXT) >= 0);
    {
        auto terminal = immediate(fixture);
        terminal.write(std::string_view{"x\n"});
        terminal.flush();
    }
    auto restored = ::_setmode(::_fileno(fixture.output.stream), _O_BINARY);
    CHECK(restored == _O_TEXT);
}

TEST(WIN_FAILURE_capture_has_no_side_effects_or_lease)
{
    Fixture fixture;
    auto in = fixture.input_mode(), out = fixture.output_mode();
    {
        esc::testing::FaultScope faults{
            {{.operation = esc::testing::Operation::GetAttributes,
              .error = std::make_error_code(std::errc::io_error)}}};
        CHECK_THROWS(std::system_error, immediate(fixture));
    }
    CHECK(fixture.input_mode() == in);
    CHECK(fixture.output_mode() == out);
    auto next = immediate(fixture);
    CHECK(next.is_open());
}
TEST(WIN_FAILURE_os_transition_rolls_back_and_remains_usable)
{
    Fixture fixture;
    auto terminal = immediate(fixture);
    auto active = fixture.input_mode();
    {
        esc::testing::FaultScope faults{
            {{.operation = esc::testing::Operation::SetAttributes,
              .error = std::make_error_code(std::errc::io_error)}}};
        CHECK_THROWS(std::system_error, terminal.set(esc::Signals::Off));
    }
    CHECK(terminal.is_open());
    CHECK(fixture.input_mode() == active);
    terminal.set(esc::Signals::Off);
    terminal.close();
}
TEST(WIN_FAILURE_partial_protocol_write_marks_session_unusable)
{
    Fixture fixture;
    auto before = fixture.input_mode();
    auto terminal = immediate(fixture);
    {
        esc::testing::FaultScope faults{
            {{.operation = esc::testing::Operation::Write,

              .error = std::make_error_code(std::errc::io_error),
              .after_bytes = 2}}};
        CHECK_THROWS(esc::SessionError, terminal.set(esc::CursorMode::Hide));
    }
    CHECK(terminal.is_open());
    CHECK_THROWS(std::logic_error, terminal.area());
    terminal.close();
    CHECK(fixture.input_mode() == before);
}
TEST(WIN_FAILURE_cleanup_continues_and_releases_ownership)
{
    Fixture fixture;
    auto before = fixture.input_mode();
    auto terminal = immediate(fixture);
    {
        esc::testing::FaultScope faults{
            {{.operation = esc::testing::Operation::Flush,
              .error = std::make_error_code(std::errc::io_error),
              .persistent = true}}};
        CHECK_THROWS(std::system_error, terminal.close());
    }
    CHECK(!terminal.is_open());
    CHECK(fixture.input_mode() == before);
    auto next = immediate(fixture);
    CHECK(next.is_open());
}
TEST(WIN_SESSION_incompatible_echo_validates_before_effects)
{
    Fixture fixture;
    auto in = fixture.input_mode(), out = fixture.output_mode();
    esc::TerminalOptions options;
    options.echo = esc::Echo::On;
    options.input_buffer = esc::InputBuffer::Immediate;
    CHECK_THROWS(std::invalid_argument,
                 esc::Terminal::open(fixture.streams(), options));
    CHECK(fixture.input_mode() == in);
    CHECK(fixture.output_mode() == out);
}

TEST(WIN_INPUT_malformed_native_surrogate_does_not_hide_next_key)
{
    Fixture fixture;
    auto terminal = immediate(fixture);
    esc::Reader reader{fixture.input.stream};
    fixture.key(0xd83d);
    fixture.key(0, VK_RIGHT);
    key(reader, char32_t{0xfffd});
    key(reader, esc::KeyCode::ArrowRight);
}
TEST(WIN_INPUT_mouse_tracking_filters_hover_and_drag)
{
    Fixture fixture;
    esc::TerminalOptions options;
    options.echo = esc::Echo::Off;
    options.input_buffer = esc::InputBuffer::Immediate;
    esc::TerminalProfile profile;
    profile.control = true;
    profile.mouse_protocol = esc::MouseProtocol::Sgr;
    profile.maximum_mouse_mode = esc::MouseMode::Move;
    options.profile = profile;
    options.mouse = esc::MouseMode::Basic;
    auto terminal = esc::Terminal::open(fixture.streams(), options);
    INPUT_RECORD event{};
    event.EventType = MOUSE_EVENT;
    event.Event.MouseEvent.dwMousePosition = {2, 1};
    event.Event.MouseEvent.dwEventFlags = MOUSE_MOVED;
    fixture.record(event);
    CHECK(terminal.read(5ms).status == esc::ReadStatus::Timeout);
    terminal.set(esc::MouseMode::Drag);
    fixture.record(event);
    CHECK(terminal.read(5ms).status == esc::ReadStatus::Timeout);
    event.Event.MouseEvent.dwButtonState = FROM_LEFT_1ST_BUTTON_PRESSED;
    fixture.record(event);
    CHECK(terminal.read(100ms).event ==
          std::optional<esc::Event>{esc::MouseMove{{2, 1}, esc::MouseButton::Left}});
    terminal.set(esc::MouseMode::Move);
    event.Event.MouseEvent.dwButtonState = 0;
    fixture.record(event);
    CHECK(terminal.read(100ms).event ==
          std::optional<esc::Event>{esc::MouseMove{{2, 1}, {}}});
}
TEST(WIN_SESSION_resize_reports_visible_viewport)
{
    Fixture fixture;
    esc::TerminalOptions options;
    options.echo = esc::Echo::Off;
    options.input_buffer = esc::InputBuffer::Immediate;
    options.resize_interval = 1ms;
    auto terminal = esc::Terminal::open(fixture.streams(), options);
    auto before = terminal.area();
    CHECK(before.width > 2);
    CONSOLE_SCREEN_BUFFER_INFO info{};
    CHECK(::GetConsoleScreenBufferInfo(fixture.output.handle(), &info));
    auto window = info.srWindow;
    --window.Right;
    CHECK(::SetConsoleWindowInfo(fixture.output.handle(), TRUE, &window));
    CHECK(terminal.area() == esc::Area{before.width - 1, before.height});
    CHECK(terminal.read(100ms).event ==
          std::optional<esc::Event>{esc::Resize{{before.width - 1, before.height}}});
}
TEST(WIN_INPUT_disk_file_reports_eof_without_polling_forever)
{
    auto* stream = std::tmpfile();
    CHECK(stream);
    CHECK(::_setmode(::_fileno(stream), _O_BINARY) >= 0);
    {
        esc::Output output{stream};
        output.write("z");
        output.flush();
    }
    std::rewind(stream);
    {
        esc::Reader reader{stream};
        key(reader, U'z');
        CHECK(reader.read(0ms).status == esc::ReadStatus::EndOfInput);
    }
    CHECK(std::fclose(stream) == 0);
}

TEST(WIN_QUERY_transport_is_explicit_and_restores_native_modes_and_codepages)
{
    Fixture fixture;
    auto mode = fixture.input_mode();
    auto codepage = ::GetConsoleCP();
    auto terminal = immediate(fixture);
    CHECK(!(fixture.input_mode() & ENABLE_VIRTUAL_TERMINAL_INPUT));
    terminal.request(esc::KeyboardQuery{});
    CHECK(fixture.input_mode() & ENABLE_VIRTUAL_TERMINAL_INPUT);
    CHECK(::GetConsoleCP() == CP_UTF8);
    fixture.key(L'x');
    CHECK(terminal.read(500ms).event == std::optional<esc::Event>{esc::KeyPress{U'x'}});
    terminal.close();
    CHECK(fixture.input_mode() == mode);
    CHECK(::GetConsoleCP() == codepage);
}
TEST(WIN_QUERY_idle_worker_is_cancelled_before_close_without_handle_leaks)
{
    Fixture fixture;
    DWORD before = 0, after = 0;
    CHECK(::GetProcessHandleCount(::GetCurrentProcess(), &before));
    for (unsigned i = 0; i < 5; ++i) {
        auto terminal = immediate(fixture);
        terminal.request(esc::KeyboardQuery{});
        // No KEY_EVENT with text: readiness must not force a blocking console read.
        fixture.key(L'x', 0, 0, 1, false);
        (void)terminal.read(1ms);
        auto start = std::chrono::steady_clock::now();
        terminal.close();
        CHECK(std::chrono::steady_clock::now() - start < 500ms);
    }
    CHECK(::GetProcessHandleCount(::GetCurrentProcess(), &after));
    CHECK(before == after);
}
TEST(WIN_QUERY_vt_reply_bytes_are_typed_and_user_input_is_preserved)
{
    Fixture fixture;
    auto terminal = immediate(fixture);
    terminal.request(esc::KeyboardQuery{});
    fixture.key(L'x');
    for (auto c : std::wstring_view{L"\x1b[?3u"})
        fixture.key(c);
    CHECK(terminal.read(500ms).event == std::optional<esc::Event>{esc::KeyPress{U'x'}});
    CHECK(terminal.read(500ms).event ==
          std::optional<esc::Event>{esc::TerminalReply{esc::KeyboardReply{3}}});
}
TEST(WIN_QUERY_failed_transport_change_rolls_back_and_remains_usable)
{
    Fixture fixture;
    auto terminal = immediate(fixture);
    auto mode = fixture.input_mode();
    auto codepage = ::GetConsoleCP();
    esc::testing::FaultScope faults{{esc::testing::Operation::SetAttributes, 1,
                                     std::make_error_code(std::errc::io_error)}};
    CHECK_THROWS(std::system_error, terminal.request(esc::KeyboardQuery{}));
    CHECK(fixture.input_mode() == mode && ::GetConsoleCP() == codepage);
    fixture.key(L'z');
    CHECK(terminal.read(500ms).event == std::optional<esc::Event>{esc::KeyPress{U'z'}});
}

TEST(WIN_QUERY_partial_native_surrogate_is_preserved_before_transport_switch)
{
    Fixture fixture;
    auto terminal = immediate(fixture);
    fixture.key(0xd83d);
    CHECK(terminal.read(0ms).status == esc::ReadStatus::Timeout);
    auto mode = fixture.input_mode();
    CHECK_THROWS(std::logic_error, terminal.request(esc::KeyboardQuery{}));
    CHECK(fixture.input_mode() == mode);
    fixture.key(0xdc7d);
    CHECK(terminal.read(500ms).event ==
          std::optional<esc::Event>{esc::KeyPress{U'👽'}});
    terminal.request(esc::KeyboardQuery{});
    CHECK(fixture.input_mode() & ENABLE_VIRTUAL_TERMINAL_INPUT);
}
