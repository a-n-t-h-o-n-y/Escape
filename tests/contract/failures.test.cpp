#include "support/fixtures.hpp"
#include "support/posix.hpp"
#include "support/faults.hpp"

using namespace esc;
using namespace esc::testing;
using namespace std::chrono_literals;

namespace {
auto streams(contract::Pty& pty) -> Streams { return {pty.input.get(),pty.output.get()}; }
auto config() -> TerminalOptions
{
    auto result=TerminalOptions::interactive();
    result.profile=contract::full_profile();
    result.resize_interval=std::nullopt;
    return result;
}
auto error(std::errc value) -> std::error_code { return std::make_error_code(value); }
}

TEST(OUT_02_short_output_transfers_are_completed_in_order)
{
    contract::File file;
    {
        FaultScope faults{{.operation=Operation::Write,.maximum_transfer=2}};
        Output{file.get()}.write("abcdef");
        CHECK(faults.calls(Operation::Write)>=3);
    }
    CHECK(file.contents()=="abcdef");
}
TEST(OUT_02_partial_output_failure_reports_error_and_keeps_prefix)
{
    contract::File file;
    FaultScope faults{{.operation=Operation::Write,.error=error(std::errc::io_error),.after_bytes=3}};
    CHECK_THROWS(std::system_error,Output{file.get()}.write("abcdef"));
    CHECK(file.contents()=="abc");
}
TEST(IO_01_interrupted_read_retries_and_preserves_events)
{
    contract::Pipe pipe;
    Reader reader{pipe.reader.get()};
    pipe.write("x");
    FaultScope faults{{.operation=Operation::Read,.error=error(std::errc::interrupted)}};
    CHECK(reader.read(100ms).event==std::optional<Event>{KeyPress{U'x'}});
    CHECK(faults.calls(Operation::Read)>=2);
}
TEST(IO_01_repeated_poll_interruptions_do_not_restart_deadline)
{
    contract::Pipe pipe;
    Reader reader{pipe.reader.get()};
    FaultScope faults{{.operation=Operation::Poll,.error=error(std::errc::interrupted),.persistent=true,.delay=1ms}};
    // CTest bounds the process: restarting the timeout would never finish.
    CHECK(reader.read(10ms).status==ReadStatus::Timeout);
}
TEST(IO_01_read_failure_keeps_original_error_code)
{
    contract::Pipe pipe;
    Reader reader{pipe.reader.get()};
    pipe.write("x");
    FaultScope faults{{.operation=Operation::Read,.error=error(std::errc::io_error)}};
    try { static_cast<void>(reader.read(100ms)); CHECK(false); }
    catch (std::system_error const& failure) { CHECK(failure.code()==std::errc::io_error); }
}
TEST(SES_03_capture_failure_changes_nothing_and_releases_ownership)
{
    contract::Pty pty;
    auto before=pty.settings();
    {
        FaultScope faults{{.operation=Operation::GetAttributes,.error=error(std::errc::io_error)}};
        CHECK_THROWS(std::system_error,Terminal::open(streams(pty),config()));
    }
    CHECK(contract::same_settings(before,pty.settings()));
    CHECK(pty.drain().empty());
    auto terminal=Terminal::open(streams(pty),config());
    terminal.close();
}
TEST(SES_03_partial_open_restores_os_state_and_releases_device)
{
    contract::Pty pty;
    auto before=pty.settings();
    {
        FaultScope faults{{.operation=Operation::Write,.error=error(std::errc::io_error),.after_bytes=2}};
        CHECK_THROWS(SessionError,Terminal::open(streams(pty),config()));
        CHECK(faults.calls(Operation::SetAttributes)>=2);
    }
    CHECK(contract::same_settings(before,pty.settings()));
    auto terminal=Terminal::open(streams(pty),config());
    terminal.close();
}
TEST(SES_03_failed_transition_marks_session_unusable_until_close)
{
    contract::Pty pty;
    auto opts=TerminalOptions{};
    opts.profile=contract::full_profile();
    opts.resize_interval=std::nullopt;
    auto terminal=Terminal::open(streams(pty),opts);
    {
        FaultScope faults{{.operation=Operation::Write,.error=error(std::errc::io_error),.after_bytes=2}};
        CHECK_THROWS(SessionError,terminal.set(Echo::Off,CursorMode::Hide));
    }
    CHECK(terminal.is_open());
    CHECK_THROWS(std::logic_error,terminal.write("x"));
    CHECK_THROWS(std::logic_error,terminal.set(Echo::On));
    CHECK_THROWS(std::logic_error,terminal.read(0ms));
    terminal.close();
    CHECK(!terminal.is_open());
}
TEST(SES_03_transition_reports_primary_and_rollback_errors)
{
    contract::Pty pty;
    auto opts=TerminalOptions{};
    opts.profile=contract::full_profile();
    auto terminal=Terminal::open(streams(pty),opts);
    FaultScope faults{
        {.operation=Operation::Write,.error=error(std::errc::io_error),.after_bytes=2},
        {.operation=Operation::SetAttributes,.on_call=2,.error=error(std::errc::permission_denied)}
    };
    try { terminal.set(Echo::Off,CursorMode::Hide); CHECK(false); }
    catch (SessionError const& failure) {
        CHECK(failure.code()==std::errc::io_error);
        CHECK(failure.rollback_error()==std::errc::permission_denied);
    }
    terminal.close();
}
TEST(SES_04_cleanup_attempts_os_restore_even_after_flush_failure)
{
    contract::Pty pty;
    auto before=pty.settings();
    auto terminal=Terminal::open(streams(pty),config());
    {
        FaultScope faults{{.operation=Operation::Flush,.error=error(std::errc::io_error)}};
        CHECK_THROWS(std::system_error,terminal.close());
        CHECK(faults.calls(Operation::SetAttributes)>0);
        CHECK(faults.calls(Operation::Write)>0);
    }
    CHECK(!terminal.is_open());
    CHECK(contract::same_settings(before,pty.settings()));
    terminal.close();
}
TEST(SES_04_destructor_swallows_cleanup_error_and_releases_lease)
{
    contract::Pty pty;
    auto before=pty.settings();
    {
        FaultScope faults{{.operation=Operation::Flush,.on_call=2,.error=error(std::errc::io_error)}};
        auto terminal=Terminal::open(streams(pty),config()); // first flush at open
    }
    CHECK(contract::same_settings(before,pty.settings()));
    auto replacement=Terminal::open(streams(pty),config());
    replacement.close();
}
TEST(SES_05_dimension_query_is_single_and_failure_is_checked)
{
    contract::Pty pty;
    auto opts=TerminalOptions{};
    opts.profile=contract::full_profile();
    auto terminal=Terminal::open(streams(pty),opts);
    {
        FaultScope trace;
        CHECK(terminal.area()==Area{80,24});
        CHECK(trace.calls(Operation::GetSize)==1);
    }
    FaultScope faults{{.operation=Operation::GetSize,.error=error(std::errc::io_error)}};
    CHECK_THROWS(std::system_error,terminal.area());
}
TEST(CAP_02_explicit_profile_avoids_environment_resolution)
{
    contract::Pty pty;
    FaultScope trace;
    auto terminal=Terminal::open(streams(pty),config());
    CHECK(trace.calls(Operation::Environment)==0);
    terminal.close();
}

TEST(SES_03_os_setting_failure_is_checked_before_protocol_writes)
{
    contract::Pty pty;
    auto opts=TerminalOptions{};
    opts.profile=contract::full_profile();
    opts.resize_interval=std::nullopt;
    auto terminal=Terminal::open(streams(pty),opts);
    auto before=pty.settings();
    {
        FaultScope faults{{.operation=Operation::SetAttributes,.error=error(std::errc::io_error)}};
        CHECK_THROWS(std::system_error,terminal.set(Echo::Off,CursorMode::Hide));
        CHECK(faults.calls(Operation::Write)==0);
    }
    CHECK(contract::same_settings(before,pty.settings()));
    CHECK(pty.drain().empty());
    terminal.close();
}
TEST(SES_04_close_error_still_marks_closed_and_releases_device_ownership)
{
    contract::Pty pty;
    auto before=pty.settings();
    auto terminal=Terminal::open(streams(pty),config());
    {
        FaultScope faults{{.operation=Operation::SetAttributes,.error=error(std::errc::io_error)}};
        CHECK_THROWS(std::system_error,terminal.close());
    }
    CHECK(!terminal.is_open());
    pty.settings(before); // Test cleanup after the intentionally failed OS restoration.
    auto replacement=Terminal::open(streams(pty),config());
    replacement.close();
}
