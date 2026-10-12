#include "support/fixtures.hpp"
#include "support/posix.hpp"

#include <thread>
#include <type_traits>

using namespace esc;
using namespace std::chrono_literals;

static_assert(std::is_move_constructible_v<Reader> && !std::is_copy_constructible_v<Reader>);

TEST(IO_01_reader_queues_multiple_events_and_distinguishes_eof)
{
    contract::Pipe pipe;
    Reader reader{pipe.reader.get()};
    pipe.write("ab界");
    pipe.eof();
    for (auto scalar : {U'a',U'b',U'界'}) {
        auto result=reader.read(100ms);
        CHECK(result.status==ReadStatus::Event);
        CHECK(result.event==std::optional<Event>{KeyPress{scalar}});
    }
    for (int i=0;i<2;++i) {
        auto result=reader.read(); // EOF is already known; cannot block forever.
        CHECK(result.status==ReadStatus::EndOfInput && !result.event);
    }
}
TEST(IO_01_short_caller_timeout_does_not_expire_a_partial_sequence)
{
    contract::Pipe pipe;
    Reader reader{pipe.reader.get(),ParserOptions{.escape_timeout=500ms,.sequence_timeout=500ms}};
    pipe.write("\x1b[");
    auto first=reader.read(1ms);
    CHECK(first.status==ReadStatus::Timeout && !first.event);
    pipe.write("A");
    auto next=reader.read(100ms);
    CHECK(next.event==std::optional<Event>{KeyPress{KeyCode::ArrowUp}});
}
TEST(IO_01_unicode_fragment_survives_multiple_timed_reads)
{
    contract::Pipe pipe;
    Reader reader{pipe.reader.get()};
    pipe.write("\xe7");
    CHECK(reader.read(1ms).status==ReadStatus::Timeout);
    pipe.write("\x95");
    CHECK(reader.read(1ms).status==ReadStatus::Timeout);
    pipe.write("\x8c");
    CHECK(reader.read(100ms).event==std::optional<Event>{KeyPress{U'界'}});
}
TEST(IO_01_zero_timeout_polls_and_negative_timeout_is_invalid)
{
    contract::Pipe pipe;
    Reader reader{pipe.reader.get()};
    auto empty=reader.read(0ms);
    CHECK(empty.status==ReadStatus::Timeout && !empty.event);
    CHECK_THROWS(std::invalid_argument,reader.read(-1ms));
    CHECK_THROWS(std::invalid_argument,Reader{nullptr});
    pipe.write("x");
    CHECK(reader.read(0ms).event==std::optional<Event>{KeyPress{U'x'}});
}
TEST(IO_01_expired_escape_is_delivered_before_caller_timeout)
{
    contract::Pipe pipe;
    Reader reader{pipe.reader.get(),ParserOptions{.escape_timeout=0ms}};
    pipe.write("\x1b");
    auto result=reader.read(100ms);
    CHECK(result.status==ReadStatus::Event);
    CHECK(result.event==std::optional<Event>{KeyPress{KeyCode::Escape}});
}
TEST(IO_01_eof_drains_partial_input_before_end_status)
{
    contract::Pipe pipe;
    Reader reader{pipe.reader.get()};
    pipe.write("\xe7\x95");
    pipe.eof();
    auto result=reader.read(100ms);
    CHECK(result.status==ReadStatus::Event);
    CHECK(std::get<UnknownInput>(*result.event).bytes=="\xe7\x95");
    CHECK(std::get<UnknownInput>(*result.event).problem==InputProblem::Truncated);
    CHECK(reader.read(100ms).status==ReadStatus::EndOfInput);
}
TEST(IO_01_reader_move_preserves_pending_input_and_borrowed_stream)
{
    contract::Pipe pipe;
    {
        Reader source{pipe.reader.get(),ParserOptions{.escape_timeout=500ms,.sequence_timeout=500ms}};
        pipe.write("\x1b[");
        CHECK(source.read(1ms).status==ReadStatus::Timeout);
        Reader destination{std::move(source)};
        pipe.write("A");
        CHECK(destination.read(100ms).event==std::optional<Event>{KeyPress{KeyCode::ArrowUp}});
    }
    CHECK(::fcntl(::fileno(pipe.reader.get()),F_GETFD)!=-1);
}
TEST(IO_01_configure_does_not_drop_queued_keys)
{
    contract::Pipe pipe;
    Reader reader{pipe.reader.get()};
    pipe.write("ab");
    CHECK(reader.read(100ms).event==std::optional<Event>{KeyPress{U'a'}});
    reader.configure(InputProtocols{.focus=true});
    CHECK(reader.read(100ms).event==std::optional<Event>{KeyPress{U'b'}});
    pipe.write("\x1b[I");
    CHECK(reader.read(100ms).event==std::optional<Event>{Focus{true}});
}
