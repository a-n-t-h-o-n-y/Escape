#include "support/fixtures.hpp"
#include "support/posix.hpp"

#include <type_traits>

using namespace esc;
using namespace std::chrono_literals;

static_assert(std::is_nothrow_move_constructible_v<Terminal>);
static_assert(!std::is_copy_constructible_v<Terminal> && !std::is_move_assignable_v<Terminal>);
static_assert(Setting<Echo> && Setting<MouseMode> && !Setting<Cursor>);

namespace {
auto options() -> TerminalOptions
{
    TerminalOptions result;
    result.profile=contract::full_profile();
    result.resize_interval=std::nullopt;
    return result;
}
auto streams(contract::Pty& pty) -> Streams { return {pty.input.get(),pty.output.get()}; }
}

TEST(SES_01_passive_session_changes_no_terminal_modes)
{
    contract::Pty pty;
    auto before=pty.settings();
    {
        auto terminal=Terminal::open(streams(pty),options());
        CHECK(terminal.is_open());
        CHECK(contract::same_settings(before,pty.settings()));
        CHECK(pty.drain().empty());
        terminal.close();
    }
    CHECK(contract::same_settings(before,pty.settings()));
    CHECK(pty.drain().empty());
}
TEST(SES_01_interactive_factory_has_minimal_explicit_defaults)
{
    auto config=TerminalOptions::interactive();
    CHECK(config.echo==std::optional{Echo::Off});
    CHECK(config.input_buffer==std::optional{InputBuffer::Immediate});
    CHECK(config.screen_buffer==std::optional{ScreenBuffer::Alternate});
    CHECK(config.cursor==std::optional{CursorMode::Hide});
    CHECK(!config.signals && !config.mouse && !config.focus && !config.paste);
}
TEST(SES_01_interactive_open_flushes_modes_and_restores_on_close)
{
    contract::Pty pty;
    auto before=pty.settings();
    auto config=TerminalOptions::interactive();
    config.profile=contract::full_profile();
    config.resize_interval=std::nullopt;
    auto terminal=Terminal::open(streams(pty),config);
    auto active=pty.settings();
    CHECK(!(active.c_lflag&(ECHO|ECHONL|ICANON|IEXTEN)));
    CHECK(!(active.c_iflag&(ICRNL|INLCR|IGNCR|ISTRIP|IXON|IXOFF)));
    CHECK(active.c_cc[VMIN]==0 && active.c_cc[VTIME]==0);
    CHECK((active.c_lflag&ISIG)==(before.c_lflag&ISIG));
    CHECK(active.c_oflag==before.c_oflag && active.c_cflag==before.c_cflag);
    CHECK(pty.drain()=="\x1b[?1049h\x1b[?25l");
    terminal.close();
    CHECK(contract::same_settings(before,pty.settings()));
    CHECK(pty.drain()=="\x1b[?25h\x1b[?1049l");
}
TEST(SES_01_flag_changes_preserve_unrelated_os_state)
{
    contract::Pty pty;
    auto baseline=pty.settings();
    baseline.c_lflag|=ECHO|ICANON|ISIG|IEXTEN;
    baseline.c_iflag|=IXON|IXOFF|ICRNL;
    pty.settings(baseline);
    baseline=pty.settings();
    auto terminal=Terminal::open(streams(pty),options());
    terminal.set(Echo::Off);
    auto off=pty.settings();
    CHECK(!(off.c_lflag&(ECHO|ECHONL)));
    CHECK((off.c_lflag&~(ECHO|ECHONL))==(baseline.c_lflag&~(ECHO|ECHONL)));
    CHECK(off.c_iflag==baseline.c_iflag);
    terminal.set(Echo::On);
    CHECK(pty.settings().c_lflag&ECHO);
    terminal.set(Signals::Off);
    CHECK(!(pty.settings().c_lflag&(ISIG|IEXTEN)));
    CHECK(!(pty.settings().c_iflag&(IXON|IXOFF)));
    terminal.set(Signals::On);
    CHECK(pty.settings().c_lflag&ISIG);
    terminal.close();
    CHECK(contract::same_settings(baseline,pty.settings()));
}
TEST(SES_01_switching_back_to_canonical_rebuilds_captured_settings)
{
    contract::Pty pty;
    auto before=pty.settings();
    auto terminal=Terminal::open(streams(pty),options());
    terminal.set(InputBuffer::Immediate);
    terminal.set(InputBuffer::Canonical);
    auto after=pty.settings();
    CHECK(after.c_lflag&ICANON);
    CHECK(after.c_iflag==before.c_iflag);
    terminal.close();
}
TEST(SES_01_mode_changes_do_not_discard_pending_input)
{
    contract::Pty pty;
    pty.send("held\n");
    auto terminal=Terminal::open(streams(pty),options());
    terminal.set(Echo::Off,InputBuffer::Immediate);
    CHECK(terminal.read(100ms).event==std::optional<Event>{KeyPress{U'h'}});
    terminal.close();
}
TEST(SES_02_invalid_duplicate_and_unsupported_settings_are_transactional)
{
    contract::Pty pty;
    auto config=options();
    config.profile->focus_reporting=false;
    auto terminal=Terminal::open(streams(pty),config);
    auto before=pty.settings();
    CHECK_THROWS(std::invalid_argument,terminal.set(Echo::Off,Echo::On));
    CHECK_THROWS(std::invalid_argument,terminal.set(Echo::Off,static_cast<Signals>(99)));
    CHECK_THROWS(UnsupportedFeature,terminal.set(Echo::Off,FocusReporting::On));
    CHECK(contract::same_settings(before,pty.settings()));
    CHECK(pty.drain().empty());
    terminal.set();
    terminal.close();
}
TEST(SES_02_repeated_alternate_setting_does_not_reenter_or_clear_screen)
{
    contract::Pty pty;
    auto terminal=Terminal::open(streams(pty),options());
    terminal.set(ScreenBuffer::Alternate);
    CHECK(pty.drain()=="\x1b[?1049h");
    terminal.set(ScreenBuffer::Alternate);
    CHECK(pty.drain().empty());
    terminal.close();
}
TEST(SES_02_single_ownership_is_by_terminal_device_not_descriptor)
{
    contract::Pty pty;
    auto first=Terminal::open(streams(pty),options());
    contract::File other_input{contract::duplicated_file(pty.slave.get(),"r")};
    contract::File other_output{contract::duplicated_file(pty.slave.get(),"w")};
    try {
        static_cast<void>(Terminal::open(Streams{other_input.get(),other_output.get()},options()));
        CHECK(false);
    } catch (std::system_error const& error) {
        CHECK(error.code()==std::errc::device_or_resource_busy);
    }
    first.close();
    auto replacement=Terminal::open(streams(pty),options());
    replacement.close();
}
TEST(SES_02_moving_transfers_cleanup_and_closed_operations_fail)
{
    contract::Pty pty;
    auto before=pty.settings();
    auto source=Terminal::open(streams(pty),options());
    source.set(Echo::Off,CursorMode::Hide);
    pty.drain();
    auto destination=std::move(source);
    CHECK(!source.is_open() && destination.is_open());
    source.close();
    CHECK(pty.drain().empty());
    destination.close();
    CHECK(!destination.is_open());
    CHECK(contract::same_settings(before,pty.settings()));
    CHECK(pty.drain()=="\x1b[?25h");
    destination.close();
    CHECK(pty.drain().empty());
    CHECK_THROWS(std::logic_error,destination.area());
    CHECK_THROWS(std::logic_error,destination.write("x"));
    CHECK_THROWS(std::logic_error,destination.send(Cursor{}));
    CHECK_THROWS(std::logic_error,destination.set(Echo::On));
    CHECK_THROWS(std::logic_error,destination.read(0ms));
    CHECK_THROWS(std::logic_error,destination.profile());
    CHECK_THROWS(std::logic_error,destination.output());
    CHECK_THROWS(std::logic_error,destination.flush());
}
TEST(SES_04_destructor_restores_during_exception_unwinding)
{
    contract::Pty pty;
    auto before=pty.settings();
    try {
        auto terminal=Terminal::open(streams(pty),options());
        terminal.set(Echo::Off,CursorMode::Hide);
        pty.drain();
        throw std::runtime_error("application exception");
    } catch (std::runtime_error const&) {}
    CHECK(contract::same_settings(before,pty.settings()));
    CHECK(pty.drain()=="\x1b[?25h");
    CHECK(::fcntl(::fileno(pty.input.get()),F_GETFD)!=-1);
    CHECK(::fcntl(::fileno(pty.output.get()),F_GETFD)!=-1);
}
TEST(SES_04_supplied_protocol_state_is_restored_instead_of_default)
{
    contract::Pty pty;
    auto config=options();
    config.restore_modes.cursor=CursorMode::Hide;
    config.restore_modes.focus=FocusReporting::On;
    config.restore_modes.paste=BracketedPaste::On;
    auto terminal=Terminal::open(streams(pty),config);
    terminal.set(CursorMode::Show,FocusReporting::Off,BracketedPaste::Off);
    CHECK(pty.drain()=="\x1b[?25h\x1b[?1004l\x1b[?2004l");
    terminal.close();
    CHECK(pty.drain()=="\x1b[?2004h\x1b[?1004h\x1b[?25l");
}
TEST(SES_04_no_unrequested_autowrap_or_style_reset)
{
    contract::Pty pty;
    auto terminal=Terminal::open(streams(pty),options());
    terminal.set(CursorMode::Hide);
    terminal.close();
    CHECK(pty.drain()=="\x1b[?25l\x1b[?25h");
}
TEST(SES_01_redirected_mismatched_and_null_streams_fail_before_changes)
{
    contract::Pty pty;
    contract::Pty other;
    contract::File file;
    auto before=pty.settings();
    CHECK_THROWS(std::system_error,Terminal::open(Streams{pty.input.get(),file.get()},options()));
    CHECK_THROWS(std::system_error,Terminal::open(Streams{pty.input.get(),other.output.get()},options()));
    CHECK_THROWS(std::invalid_argument,Terminal::open(Streams{nullptr,pty.output.get()},options()));
    CHECK(contract::same_settings(before,pty.settings()));
    CHECK(pty.drain().empty());
}
TEST(SES_05_area_reads_one_size_snapshot_and_accepts_zero)
{
    contract::Pty pty;
    auto terminal=Terminal::open(streams(pty),options());
    CHECK(terminal.area()==Area{80,24});
    pty.resize(123,45);
    CHECK(terminal.area()==Area{123,45});
    pty.resize(0,0);
    CHECK(terminal.area()==Area{0,0});
}
TEST(SES_05_resize_events_only_report_changed_observations)
{
    contract::Pty pty;
    auto config=options();
    config.input_buffer=InputBuffer::Immediate;
    config.echo=Echo::Off;
    config.resize_interval=1ms;
    auto terminal=Terminal::open(streams(pty),config);
    CHECK(terminal.read(0ms).status==ReadStatus::Timeout); // No initial Resize.
    pty.resize(100,40);
    CHECK(terminal.read(100ms).event==std::optional<Event>{Resize{{100,40}}});
    pty.resize(100,40);
    CHECK(terminal.read(5ms).status==ReadStatus::Timeout);
    pty.resize(110,45);
    pty.resize(120,50);
    CHECK(terminal.read(100ms).event==std::optional<Event>{Resize{{120,50}}});
}
TEST(SES_05_disabled_resize_polling_does_not_emit_resize_events)
{
    contract::Pty pty;
    auto config=options();
    config.input_buffer=InputBuffer::Immediate;
    auto terminal=Terminal::open(streams(pty),config);
    pty.resize(100,40);
    CHECK(terminal.read(5ms).status==ReadStatus::Timeout);
    CHECK_THROWS(std::invalid_argument,Terminal::open(streams(pty),TerminalOptions{.resize_interval=0ms}));
}
TEST(SES_01_send_is_capability_aware_and_write_remains_raw)
{
    contract::Pty pty;
    auto config=options();
    config.profile->colors=ColorSupport::Basic8;
    auto terminal=Terminal::open(streams(pty),config);
    terminal.send(Cursor{},fg(TColor{0x800000}));
    terminal.write("\x1b[38;2;1;2;3m");
    terminal.flush();
    CHECK(pty.drain()=="\x1b[1;1H\x1b[31m\x1b[38;2;1;2;3m");
}
TEST(CAP_03_explicit_profile_overrides_environment_and_no_color)
{
    contract::EnvironmentVariable term{"TERM","dumb"};
    contract::EnvironmentVariable no_color{"NO_COLOR","1"};
    contract::Pty pty;
    auto terminal=Terminal::open(streams(pty),options());
    CHECK(terminal.profile().colors==ColorSupport::TrueColor);
    for (auto const& decision : terminal.profile_report().decisions) CHECK(decision.source==EvidenceSource::Explicit);
    terminal.send(fg(TColor{0x010203}));
    terminal.flush();
    CHECK(pty.drain()=="\x1b[38;2;1;2;3m");
}
TEST(CAP_03_environment_snapshots_own_current_values)
{
    contract::EnvironmentVariable term{"TERM","xterm"};
    contract::EnvironmentVariable color{"COLORTERM","truecolor"};
    contract::EnvironmentVariable no_color{"NO_COLOR",""};
    auto snapshot=snapshot_environment();
    CHECK(snapshot.term=="xterm" && snapshot.colorterm=="truecolor" && !snapshot.no_color);
    CHECK(::setenv("TERM","dumb",1)==0);
    CHECK(::setenv("NO_COLOR","1",1)==0);
    auto current=snapshot_environment();
    CHECK(snapshot.term=="xterm" && !snapshot.no_color);
    CHECK(current.term=="dumb" && current.no_color);
}

TEST(SES_01_read_and_send_do_not_implicitly_flush_drawing)
{
    std::array<char,4096> buffer{};
    contract::Pty pty;
    CHECK(std::setvbuf(pty.output.get(),buffer.data(),_IOFBF,buffer.size())==0);
    auto config=options();
    config.echo=Echo::Off;
    config.input_buffer=InputBuffer::Immediate;
    auto terminal=Terminal::open(streams(pty),config);
    terminal.write("buffered");
    terminal.send(Cursor{});
    pty.send("x");
    CHECK(terminal.read(100ms).event==std::optional<Event>{KeyPress{U'x'}});
    CHECK(pty.drain().empty());
    terminal.flush();
    CHECK(pty.drain()=="buffered\x1b[1;1H");
}
TEST(SES_01_send_validates_the_entire_pack_before_output)
{
    contract::Pty pty;
    auto terminal=Terminal::open(streams(pty),options());
    CHECK_THROWS(std::invalid_argument,terminal.send(Cursor{},Cursor{-1,0}));
    terminal.flush();
    CHECK(pty.drain().empty());
    terminal.send();
    terminal.write();
}
TEST(SES_04_session_preserves_process_signal_dispositions)
{
    int const signals[]={SIGINT,SIGTERM,SIGWINCH};
    std::array<struct sigaction,3> before{};
    for (std::size_t i=0;i<3;++i) CHECK(::sigaction(signals[i],nullptr,&before[i])==0);
    contract::Pty pty;
    auto terminal=Terminal::open(streams(pty),options());
    for (std::size_t i=0;i<3;++i) {
        struct sigaction current{};
        CHECK(::sigaction(signals[i],nullptr,&current)==0);
        CHECK(current.sa_flags==before[i].sa_flags);
        if (current.sa_flags&SA_SIGINFO) CHECK(current.sa_sigaction==before[i].sa_sigaction);
        else CHECK(current.sa_handler==before[i].sa_handler);
        for (int signal=1;signal<NSIG;++signal)
            CHECK(::sigismember(&current.sa_mask,signal)==::sigismember(&before[i].sa_mask,signal));
    }
    terminal.close();
}
TEST(SES_05_queued_input_precedes_resize_without_reordering_keys)
{
    contract::Pty pty;
    auto config=options();
    config.echo=Echo::Off;
    config.input_buffer=InputBuffer::Immediate;
    config.resize_interval=1ms;
    auto terminal=Terminal::open(streams(pty),config);
    pty.send("ab");
    CHECK(terminal.read(100ms).event==std::optional<Event>{KeyPress{U'a'}});
    pty.resize(100,40);
    CHECK(terminal.read(100ms).event==std::optional<Event>{KeyPress{U'b'}});
    CHECK(terminal.read(100ms).event==std::optional<Event>{Resize{{100,40}}});
}
