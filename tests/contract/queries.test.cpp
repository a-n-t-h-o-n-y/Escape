#include <esc/esc.hpp>
#include <thread>
#include <vector>
#include "support/capabilities.hpp"
#include "support/faults.hpp"
#include "support/posix.hpp"
using namespace esc;
using namespace std::chrono_literals;
using namespace std::literals;
namespace {
auto options() -> TerminalOptions
{
    TerminalOptions options;
    options.profile = contract::full_profile();
    options.echo = Echo::Off;
    options.input_buffer = InputBuffer::Immediate;
    return options;
}
auto open(contract::Pty& pty) -> Terminal
{ return Terminal::open({pty.input.get(), pty.output.get()}, options()); }
struct Responder {
    std::exception_ptr error;
    std::thread thread;
    Responder(contract::Pty& pty,
              std::vector<std::pair<std::string, std::string>> stages)
        : thread{[&, stages = std::move(stages)] {
              try {
                  std::string pending;
                  for (auto const& stage : stages) {
                      auto until = std::chrono::steady_clock::now() + 2s;
                      while (pending.find(stage.first) == std::string::npos) {
                          CHECK(std::chrono::steady_clock::now() < until);
                          pending += pty.drain();
                          std::this_thread::sleep_for(1ms);
                      }
                      auto at = pending.find(stage.first);
                      pending.erase(0, at + stage.first.size());
                      pty.send(stage.second);
                  }
              }
              catch (...) {
                  error = std::current_exception();
              }
          }}
    {}
    ~Responder()
    {
        if (thread.joinable()) thread.join();
    }
    void check()
    {
        thread.join();
        if (error) std::rethrow_exception(error);
    }
};
}  // namespace
TEST(SES_QUERY_matches_reply_and_preserves_interleaved_input)
{
    contract::Pty pty;
    auto terminal = open(pty);
    Responder responder{pty, {{"\x1b[?2004$p", "a\x1b[?25;2$y\x1b[?2004;1$yb"}}};
    auto result = terminal.query(ModeQuery{2004}, 500ms);
    responder.check();
    CHECK(result.status == QueryStatus::Reply);
    CHECK(result.reply == std::optional<Reply>{ModeReply{2004, ModeStatus::Set}});
    CHECK(terminal.read(0ms).event == std::optional<Event>{KeyPress{U'a'}});
    CHECK(terminal.read(0ms).event ==
          std::optional<Event>{TerminalReply{ModeReply{25, ModeStatus::Reset}}});
    CHECK(terminal.read(0ms).event == std::optional<Event>{KeyPress{U'b'}});
}
TEST(SES_QUERY_timeout_keeps_input_and_late_replies_visible)
{
    contract::Pty pty;
    auto terminal = open(pty);
    pty.send("abc");
    auto before = std::chrono::steady_clock::now();
    CHECK(terminal.query(KeyboardQuery{}, 10ms).status == QueryStatus::Timeout);
    CHECK(std::chrono::steady_clock::now() - before < 500ms);
    for (auto c : U"abc"sv)
        CHECK(terminal.read(0ms).event == std::optional<Event>{KeyPress{c}});
    pty.send("\x1b[?0u");
    CHECK(terminal.read(100ms).event ==
          std::optional<Event>{TerminalReply{KeyboardReply{0}}});
}
TEST(SES_QUERY_preservation_storage_is_bounded_without_dropping_input)
{
    contract::Pty pty;
    auto terminal = open(pty);
    pty.send(std::string(65, 'x') + "\x1b[?1u");
    CHECK(terminal.query(KeyboardQuery{}, 100ms).status == QueryStatus::InputPending);
    CHECK(terminal.query(KeyboardQuery{}, 100ms).status == QueryStatus::InputPending);
    for (unsigned i = 0; i < 65; ++i)
        CHECK(terminal.read(100ms).event == std::optional<Event>{KeyPress{U'x'}});
    CHECK(terminal.read(100ms).event ==
          std::optional<Event>{TerminalReply{KeyboardReply{1}}});
}
TEST(SES_QUERY_request_is_explicit_and_validates_before_writes)
{
    contract::Pty pty;
    auto terminal = open(pty);
    CHECK(pty.drain().empty());
    CHECK_THROWS(std::invalid_argument, terminal.query(KeyboardQuery{}, -1ms));
    CHECK_THROWS(std::invalid_argument, terminal.request(ModeQuery{0}));
    CHECK(pty.drain().empty());
    terminal.request(DeviceAttributesQuery{});
    CHECK(pty.drain() == "\x1b[c");
    pty.send("\x1b[?1;2c");
    CHECK(terminal.read(100ms).event ==
          std::optional<Event>{TerminalReply{DeviceAttributesReply{{1, 2}}}});
}
TEST(SES_QUERY_canonical_input_is_rejected_before_output)
{
    contract::Pty pty;
    auto setting = options();
    setting.input_buffer = InputBuffer::Canonical;
    auto terminal = Terminal::open({pty.input.get(), pty.output.get()}, setting);
    CHECK_THROWS(std::invalid_argument, terminal.query(KeyboardQuery{}, 10ms));
    CHECK(pty.drain().empty());
}
TEST(SES_QUERY_probe_batches_requests_and_keeps_evidence_separate)
{
    contract::Pty pty;
    auto terminal = open(pty);
    auto before = terminal.profile().colors;
    Responder responder{pty, {{"\x1b[?2026$p", "x\x1b[?2004;0$y\x1b[?25;2$y"}}};
    testing::FaultScope operations{};
    auto report = terminal.probe(30ms);
    CHECK(operations.calls(testing::Operation::Write) == 1);
    CHECK(operations.calls(testing::Operation::Flush) == 1);
    responder.check();
    CHECK(report.observations.size() == 10);
    CHECK(report.observations[0].result.status == QueryStatus::Timeout);
    CHECK(report.observations[1].result.reply ==
          std::optional<Reply>{ModeReply{25, ModeStatus::Reset}});
    CHECK(report.observations[8].result.reply ==
          std::optional<Reply>{ModeReply{2004, ModeStatus::Unrecognized}});
    for (auto const& observation : report.observations)
        CHECK(observation.source == EvidenceSource::Probe);
    CHECK(terminal.profile().colors == before);
    CHECK(terminal.read(0ms).event == std::optional<Event>{KeyPress{U'x'}});
}
TEST(SES_KITTY_negotiates_events_and_restores_prior_flags_on_close)
{
    contract::Pty pty;
    auto terminal = open(pty);
    Responder responder{
        pty, {{"\x1b[?u", "\x1b[?0u"}, {"\x1b[=3;1u\x1b[?u", "\x1b[97;1:2u\x1b[?3u"}}};
    auto result = terminal.enable_keyboard(
        KittyFlags::Disambiguate | KittyFlags::EventTypes, 500ms);
    responder.check();
    CHECK(result.status == QueryStatus::Reply);
    CHECK(terminal.keyboard_protocol() == KeyboardProtocol::Kitty);
    KeyEvent key;
    key.key = Key{U'a'};
    key.action = KeyAction::Repeat;
    CHECK(terminal.read(0ms).event == std::optional<Event>{key});
    CHECK_THROWS(std::logic_error, terminal.set(ScreenBuffer::Alternate));
    terminal.close();
    CHECK(pty.drain() == "\x1b[=0;1u");
}
TEST(SES_KITTY_rejected_or_missing_confirmation_rolls_back)
{
    for (bool timeout : {false, true}) {
        contract::Pty pty;
        auto terminal = open(pty);
        Responder responder{pty,
                            {{"\x1b[?u", "\x1b[?0u"},
                             {"\x1b[=3;1u\x1b[?u", timeout ? "x" : "x\x1b[?1u"}}};
        auto result = terminal.enable_keyboard(
            KittyFlags::Disambiguate | KittyFlags::EventTypes, 50ms);
        responder.check();
        CHECK(result.status ==
              (timeout ? QueryStatus::Timeout : QueryStatus::Rejected));
        CHECK(terminal.keyboard_protocol() == KeyboardProtocol::Legacy);
        CHECK(terminal.read(0ms).event == std::optional<Event>{KeyPress{U'x'}});
        CHECK(pty.drain() == "\x1b[=0;1u");
        terminal.close();
        CHECK(pty.drain().empty());
    }
}
TEST(SES_KITTY_disable_restores_inherited_flags_and_allows_screen_changes)
{
    contract::Pty pty;
    auto terminal = open(pty);
    Responder responder{pty,
                        {{"\x1b[?u", "\x1b[?1u"}, {"\x1b[=3;1u\x1b[?u", "\x1b[?3u"}}};
    CHECK(terminal
              .enable_keyboard(KittyFlags::Disambiguate | KittyFlags::EventTypes, 500ms)
              .status == QueryStatus::Reply);
    responder.check();
    terminal.disable_keyboard();
    CHECK(pty.drain() == "\x1b[=1;1u");
    CHECK(terminal.keyboard_protocol() == KeyboardProtocol::Kitty);
    terminal.disable_keyboard();
    CHECK(pty.drain().empty());
    terminal.set(ScreenBuffer::Alternate);
    CHECK(pty.drain() == "\x1b[?1049h");
}
TEST(SES_KITTY_invalid_flags_and_no_support_do_not_change_keyboard_modes)
{
    contract::Pty pty;
    auto terminal = open(pty);
    CHECK_THROWS(std::invalid_argument,
                 terminal.enable_keyboard(static_cast<KittyFlags>(0)));
    CHECK_THROWS(std::invalid_argument,
                 terminal.enable_keyboard(static_cast<KittyFlags>(32)));
    CHECK_THROWS(std::invalid_argument,
                 terminal.enable_keyboard(KittyFlags::AssociatedText));
    CHECK(pty.drain().empty());
    CHECK(terminal.enable_keyboard(KittyFlags::Disambiguate, 1ms).status ==
          QueryStatus::Timeout);
    CHECK(pty.drain() == "\x1b[?u");
    CHECK(terminal.keyboard_protocol() == KeyboardProtocol::Legacy);
}
TEST(SES_KITTY_partial_mode_write_is_unusable_and_cleanup_still_restores)
{
    contract::Pty pty;
    auto terminal = open(pty);
    Responder responder{pty, {{"\x1b[?u", "\x1b[?0u"}}};
    testing::FaultScope faults{
        {testing::Operation::Write, 2, std::make_error_code(std::errc::io_error)}};
    CHECK_THROWS(SessionError,
                 terminal.enable_keyboard(KittyFlags::Disambiguate, 500ms));
    responder.check();
    CHECK(terminal.is_open());
    CHECK_THROWS(std::logic_error, terminal.read(0ms));
    terminal.close();
    CHECK(pty.drain() == "\x1b[=0;1u\x1b[=0;1u");
}

TEST(SES_QUERY_batch_validates_all_requests_before_transport_or_output)
{
    contract::Pty pty;
    auto terminal = open(pty);
    Query invalid[] = {KeyboardQuery{}, ModeQuery{0}};
    CHECK_THROWS(std::invalid_argument,
                 terminal.request(std::span<Query const>{invalid}));
    CHECK(pty.drain().empty());
    Query valid[] = {KeyboardQuery{}, DeviceAttributesQuery{}, ModeQuery{25}};
    testing::FaultScope operations{};
    terminal.request(std::span<Query const>{valid});
    CHECK(operations.calls(testing::Operation::Write) == 1);
    CHECK(operations.calls(testing::Operation::Flush) == 1);
    CHECK(pty.drain() == "\x1b[?u\x1b[c\x1b[?25$p");
}
TEST(SES_KITTY_renegotiation_restores_the_original_session_baseline)
{
    contract::Pty pty;
    auto terminal = open(pty);
    Responder responder{pty,
                        {{"\x1b[?u", "\x1b[?0u"},
                         {"\x1b[=1;1u\x1b[?u", "\x1b[?1u"},
                         {"\x1b[?u", "\x1b[?1u"},
                         {"\x1b[=3;1u\x1b[?u", "\x1b[?3u"}}};
    CHECK(terminal.enable_keyboard(KittyFlags::Disambiguate, 500ms).status ==
          QueryStatus::Reply);
    CHECK(terminal
              .enable_keyboard(KittyFlags::Disambiguate | KittyFlags::EventTypes, 500ms)
              .status == QueryStatus::Reply);
    responder.check();
    terminal.disable_keyboard();
    CHECK(pty.drain() == "\x1b[=0;1u");
    CHECK(terminal.keyboard_protocol() == KeyboardProtocol::Legacy);
}

TEST(SES_KITTY_rejection_preserves_inherited_protocol_interpretation)
{
    contract::Pty pty;
    auto terminal = open(pty);
    Responder responder{pty,
                        {{"\x1b[?u", "\x1b[?1u"}, {"\x1b[=3;1u\x1b[?u", "\x1b[?1u"}}};
    CHECK(terminal
              .enable_keyboard(KittyFlags::Disambiguate | KittyFlags::EventTypes, 500ms)
              .status == QueryStatus::Rejected);
    responder.check();
    CHECK(terminal.keyboard_protocol() == KeyboardProtocol::Kitty);
    CHECK(pty.drain() == "\x1b[=1;1u");
    pty.send("\x1b[97u");
    KeyEvent key;
    key.key = Key{U'a'};
    CHECK(terminal.read(100ms).event == std::optional<Event>{key});
    terminal.close();
    CHECK(pty.drain().empty());
}
