#include <esc/esc.hpp>
#include <optional>
#include <thread>
#include "support/allocations.hpp"
#include "support/capabilities.hpp"
#include "support/faults.hpp"
#include "support/posix.hpp"
#include "support/results.hpp"
using namespace esc;
using namespace std::chrono_literals;

namespace {
auto opts(bool synchronized = true) -> TerminalOptions
{
    TerminalOptions o;
    o.profile = contract::full_profile();
    o.profile->synchronized_output = synchronized;
    o.profile->window_title = true;
    o.profile->hyperlinks = true;
    o.profile->cursor_style = true;
    o.profile->cursor_save_restore = true;
    o.profile->scrolling = true;
    o.profile->editing = true;
    o.echo = Echo::Off;
    o.input_buffer = InputBuffer::Immediate;
    return o;
}

auto open(contract::Pty& pty, bool sync = true) -> Terminal
{ return Terminal::open({pty.input.get(), pty.output.get()}, opts(sync)); }
}  // namespace

TEST(SES_EXT_frames_nest_and_only_final_close_presents)
{
    contract::Pty pty;
    auto terminal = open(pty);
    auto outer = terminal.frame();
    CHECK(outer.synchronized());
    CHECK(pty.drain() == "\x1b[?2026h");
    auto original = terminal.frame();
    auto inner = std::move(original);
    CHECK(!original.synchronized());
    original.close();
    terminal.write("abc");
    outer.close();
    CHECK(pty.drain().empty());
    inner.close();
    CHECK(pty.drain() == "abc\x1b[?2026l");
    inner.close();
    CHECK(pty.drain().empty());
}

TEST(SES_EXT_frames_unwind_exceptions_and_follow_terminal_moves)
{
    contract::Pty pty;
    auto terminal = open(pty);
    try {
        auto frame = terminal.frame();
        auto moved = std::move(terminal);
        moved.write("x");
        throw std::runtime_error{"test"};
    }
    catch (std::runtime_error const&) {
    }
    CHECK(pty.drain() == "\x1b[?2026hx\x1b[?2026l");
}

TEST(SES_EXT_frame_guard_can_outlive_terminal_and_close)
{
    contract::Pty pty;
    std::optional<Frame> guard;
    {
        auto terminal = open(pty);
        guard.emplace(terminal.frame());
        terminal.write("x");
        terminal.close();
        CHECK(!guard->synchronized());
    }
    CHECK(pty.drain() == "\x1b[?2026hx\x1b[?2026l");
    guard->close();
    guard.reset();
    CHECK(pty.drain().empty());
}

TEST(SES_EXT_inherited_synchronization_is_preserved)
{
    contract::Pty pty;
    auto o = opts();
    o.restore_modes.synchronized_output = SynchronizedOutput::On;
    auto terminal = Terminal::open({pty.input.get(), pty.output.get()}, o);
    {
        auto frame = terminal.frame();
        CHECK(frame.synchronized());
        terminal.write("x");
    }
    terminal.close();
    CHECK(pty.drain() == "x");
}

TEST(SES_EXT_frame_fallback_and_strict_rejection)
{
    contract::Pty pty;
    {
        auto terminal = open(pty, false);
        auto frame = terminal.frame();
        CHECK(!frame.synchronized());
        terminal.write("plain");
        frame.close();
        CHECK(pty.drain() == "plain");
    }
    auto o = opts(false);
    o.profile->fallback = FallbackPolicy::Strict;
    auto terminal = Terminal::open({pty.input.get(), pty.output.get()}, o);
    CHECK_THROWS(UnsupportedFeature, terminal.frame());
    CHECK(pty.drain().empty());
}

TEST(SES_EXT_frame_blocks_screen_changes_before_output)
{
    contract::Pty pty;
    auto terminal = open(pty);
    auto frame = terminal.frame();
    pty.drain();
    CHECK_THROWS(std::logic_error, terminal.set(ScreenBuffer::Alternate));
    CHECK(pty.drain().empty());
    terminal.set(ScreenBuffer::Normal);
    frame.close();
    terminal.set(ScreenBuffer::Alternate);
}

TEST(SES_EXT_frame_partial_begin_rolls_back_and_rejects_further_work)
{
    contract::Pty pty;
    auto terminal = open(pty);
    {
        esc::testing::FaultScope faults{{esc::testing::Operation::Write, 1,
                                         std::make_error_code(std::errc::io_error),
                                         SIZE_MAX, 3}};
        CHECK_THROWS(SessionError, terminal.frame());
    }
    CHECK_THROWS(std::logic_error, terminal.write("x"));
    terminal.close();
    CHECK(pty.drain().ends_with("\x1b[?2026l"));
}

TEST(SES_EXT_frame_end_failure_is_retried_by_terminal_cleanup)
{
    contract::Pty pty;
    auto terminal = open(pty);
    auto frame = terminal.frame();
    pty.drain();
    {
        esc::testing::FaultScope faults{{esc::testing::Operation::Write, 1,
                                         std::make_error_code(std::errc::io_error)}};
        CHECK_THROWS(std::system_error, frame.close());
    }
    CHECK_THROWS(std::logic_error, terminal.write("x"));
    terminal.close();
    CHECK(pty.drain() == "\x1b[?2026l");
}

TEST(SES_EXT_frame_creation_and_nesting_allocate_nothing_after_open)
{
    contract::Pty pty;
    auto terminal = open(pty);
    contract::AllocationScope scope{true};
    auto first = terminal.frame();
    auto second = terminal.frame();
    second.close();
    first.close();
    CHECK(scope.count() == 0);
}

TEST(SES_EXT_explicit_probe_confirms_sync_and_preserves_ordinary_input)
{
    contract::Pty pty;
    auto terminal = open(pty, false);
    std::thread reply{[&] {
        std::this_thread::sleep_for(10ms);
        pty.write("x\x1b[?2026;2$y");
    }};
    auto result = terminal.confirm_synchronized_output(200ms);
    reply.join();
    CHECK(contract::status(result) == contract::Status::Reply);
    CHECK(contract::event(terminal.read(0ms)) == std::optional<Event>{KeyEvent{U'x'}});
    {
        auto frame = terminal.frame();
        CHECK(frame.synchronized());
        CHECK_THROWS(std::logic_error, terminal.confirm_synchronized_output(0ms));
    }
    CHECK(pty.drain() == "\x1b[?2026$p\x1b[?2026h\x1b[?2026l");
}

TEST(SES_EXT_permanent_or_unrecognized_sync_is_rejected)
{
    for (auto status : {0, 3, 4}) {
        contract::Pty pty;
        auto terminal = open(pty);
        std::thread reply{[&] {
            std::this_thread::sleep_for(10ms);
            pty.write("\x1b[?2026;" + std::to_string(status) + "$y");
        }};
        auto result = terminal.confirm_synchronized_output(200ms);
        reply.join();
        CHECK(contract::status(result) == contract::Status::Rejected);
        {
            auto frame = terminal.frame();
            CHECK(!frame.synchronized());
        }
    }
}

TEST(SES_EXT_long_osc_payloads_stream_without_an_intermediate_string)
{
    contract::Pty pty;
    auto terminal = open(pty);
    std::string title(300, 't'), uri = "https://example.com/" + std::string(300, 'u');
    {
        contract::AllocationScope scope{true};
        terminal.write(WindowTitle{title}, BeginHyperlink{uri, "x"}, EndHyperlink{});
        terminal.flush();
        CHECK(scope.count() == 0);
    }
    CHECK(pty.drain() ==
          encode(WindowTitle{title}, BeginHyperlink{uri, "x"}, EndHyperlink{}));
    CHECK_THROWS(std::invalid_argument, terminal.write(SaveCursor{}, WindowTitle{"ok"},
                                                       BeginHyperlink{"\x1b"}));
    CHECK(pty.drain().empty());
}

TEST(IO_EXT_wait_state_preserves_read_ahead_and_eof)
{
    contract::Pipe pipe;
    Reader reader{pipe.reader.get()};
    auto idle = reader.wait_state();
    CHECK(!idle.ready && !idle.deadline);
    CHECK(idle.source.kind == WaitSourceKind::FileDescriptor);
    CHECK(idle.source.value == fileno(pipe.reader.get()));
    pipe.write("ab");
    pipe.eof();
    CHECK(reader.wait_state().ready);
    CHECK(reader.wait_state().ready);
    CHECK(contract::event(reader.read(0ms)) == std::optional<Event>{KeyEvent{U'a'}});
    CHECK_THROWS(std::invalid_argument, reader.read(-1ms));
    CHECK(reader.wait_state().ready);
    CHECK(contract::event(reader.read(0ms)) == std::optional<Event>{KeyEvent{U'b'}});
    CHECK(reader.wait_state().ready);
    CHECK(contract::status(reader.read(0ms)) == contract::Status::EndOfInput);
    CHECK(reader.wait_state().ready);
    CHECK(contract::status(reader.read()) == contract::Status::EndOfInput);
}

TEST(IO_EXT_parser_deadlines_are_absolute_and_survive_repeated_snapshots)
{
    contract::Pipe pipe;
    Reader reader{pipe.reader.get(),
                  ParserOptions{.escape_timeout = 30ms, .sequence_timeout = 100ms}};
    pipe.write("\x1b");
    auto first = reader.wait_state();
    CHECK(!first.ready && first.deadline);
    auto second = reader.wait_state();
    CHECK(second.deadline == first.deadline);
    std::this_thread::sleep_until(*first.deadline + 1ms);
    CHECK(reader.wait_state().ready);
    CHECK(contract::event(reader.read(0ms)) ==
          std::optional<Event>{KeyEvent{KeyCode::Escape}});
    CHECK(!reader.wait_state().deadline);
}

TEST(IO_EXT_host_poll_can_combine_terminal_input_and_shutdown_pipe)
{
    contract::Pipe input, stop;
    Reader reader{input.reader.get()};
    auto state = reader.wait_state();
    pollfd fds[] = {{static_cast<int>(state.source.value), POLLIN, 0},
                    {fileno(stop.reader.get()), POLLIN, 0}};
    stop.write("s");
    CHECK(::poll(fds, 2, 100) == 1);
    CHECK(fds[1].revents & POLLIN);
    CHECK(!reader.wait_state().ready);
    input.write("x");
    CHECK(::poll(fds, 2, 100) == 2);
    CHECK(reader.wait_state().ready);
    CHECK(contract::event(reader.read(0ms)) == std::optional<Event>{KeyEvent{U'x'}});
}

TEST(SES_EXT_wait_state_exposes_resize_and_queued_probe_input)
{
    contract::Pty pty;
    auto o = opts();
    o.resize_interval = 10ms;
    auto terminal = Terminal::open({pty.input.get(), pty.output.get()}, o);
    auto idle = terminal.wait_state();
    CHECK(!idle.ready && idle.deadline);
    pty.resize(90, 30);
    std::this_thread::sleep_until(*idle.deadline + 1ms);
    CHECK(terminal.wait_state().ready);
    CHECK(contract::event(terminal.read(0ms)) ==
          std::optional<Event>{Resize{Area{90, 30}}});
    pty.write("x");
    CHECK(contract::status(terminal.query(ModeQuery{25}, 10ms)) ==
          contract::Status::Timeout);
    CHECK(terminal.wait_state().ready);
    CHECK(contract::event(terminal.read(0ms)) == std::optional<Event>{KeyEvent{U'x'}});
}

TEST(IO_EXT_wait_snapshot_is_allocation_free_and_moves_with_reader)
{
    contract::Pipe pipe;
    Reader reader{pipe.reader.get()};
    pipe.write("a");
    reader.wait_state();
    auto moved = std::move(reader);
    CHECK_THROWS(std::logic_error, reader.wait_state());
    {
        contract::AllocationScope scope{true};
        CHECK(moved.wait_state().ready);
        CHECK(contract::status(moved.read(0ms)) == contract::Status::Event);
        CHECK(!moved.wait_state().ready);
        CHECK(scope.count() == 0);
    }
}

TEST(IO_EXT_incomplete_large_units_do_not_hide_remaining_kernel_readiness)
{
    contract::File file;
    auto* stream = file.get();
    std::string input = "\x1b[200~" + std::string(6000, 'p') + "\x1b[201~";
    CHECK(std::fwrite(input.data(), 1, input.size(), stream) == input.size());
    CHECK(std::fflush(stream) == 0);
    std::rewind(stream);
    ParserOptions parser;
    parser.paste_chunk_size = 4096;
    Reader reader{stream, parser, InputProtocols{.bracketed_paste = true}};
    // First poll consumes only the bounded read-ahead block, not a paste event.
    CHECK(reader.wait_state().ready);
    auto event = reader.read(0ms);
    CHECK(contract::status(event) == contract::Status::Event);
    auto paste = std::get<PasteChunk>(*contract::event(event));
    CHECK(paste.bytes == std::string(4096, 'p'));
    auto tail = reader.read(0ms);
    CHECK(std::get<PasteChunk>(*contract::event(tail)).bytes == std::string(1904, 'p'));
}

TEST(SES_EXT_frame_guard_unwinds_while_terminal_remains_usable)
{
    contract::Pty pty;
    auto terminal = open(pty);
    try {
        auto guard = terminal.frame();
        terminal.write("x");
        throw std::runtime_error{"test"};
    }
    catch (std::runtime_error const&) {
    }
    CHECK(terminal.is_open());
    terminal.write("y");
    terminal.flush();
    CHECK(pty.drain() == "\x1b[?2026hx\x1b[?2026ly");
}

TEST(SES_EXT_confirmed_on_baseline_and_timeout_preserve_inherited_mode)
{
    contract::Pty pty;
    auto terminal = open(pty, false);
    std::thread reply{[&] {
        std::this_thread::sleep_for(10ms);
        pty.write("\x1b[?2026;1$y");
    }};
    auto result = terminal.confirm_synchronized_output(200ms);
    reply.join();
    CHECK(contract::status(result) == contract::Status::Reply);
    CHECK(!terminal.profile().synchronized_output);
    CHECK(contract::status(terminal.confirm_synchronized_output(1ms)) ==
          contract::Status::Timeout);
    {
        auto frame = terminal.frame();
        CHECK(frame.synchronized());
        terminal.write("inherited");
    }
    terminal.close();
    CHECK(pty.drain() == "\x1b[?2026$p\x1b[?2026$pinherited");
}

TEST(SES_SOUND_profile_controls_bell_and_playback_without_changing_modes)
{
    contract::Pty pty;
    auto options = opts(false);
    options.profile->bell = true;
    options.profile->play_notes = true;
    auto terminal = Terminal::open({pty.input.get(), pty.output.get()}, options);
    static_cast<void>(pty.drain());
    terminal.write("ready", Bell{}, PlayNotes{.notes = {Note::C5, Note::E5, Note::G5}});
    terminal.flush();
    CHECK(pty.drain() == "ready\x07\x1b[4;4;1;5;8,~");
    CHECK(terminal.profile_report().decisions.size() == 17);
    CHECK(terminal.profile_report().decisions[15].feature == Feature::Bell);
    CHECK(terminal.profile_report().decisions[16].feature == Feature::PlayNotes);
    auto no_sound = opts(false);
    contract::Pty quiet_pty;
    auto quiet =
        Terminal::open({quiet_pty.input.get(), quiet_pty.output.get()}, no_sound);
    static_cast<void>(quiet_pty.drain());
    quiet.write("ready", Bell{}, PlayNotes{.notes = {Note::C5}});
    quiet.flush();
    CHECK(quiet_pty.drain() == "ready");
}
