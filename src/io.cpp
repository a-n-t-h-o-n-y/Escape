#include <algorithm>
#include <array>
#include <cerrno>
#include <esc/io.hpp>
#include <system_error>
#include "detail/backend.hpp"

namespace esc {
namespace {
void io_error(char const* message)
{ throw std::system_error{errno ? errno : EIO, std::generic_category(), message}; }
}  // namespace
Output::Output(std::FILE* stream) : stream_{stream}
{
    if (!stream) throw std::invalid_argument{"Output: null stream"};
}
void Output::write_one(std::string_view bytes)
{
    while (!bytes.empty()) {
        errno = 0;
        auto count = detail::write_bytes(stream_, bytes.data(), bytes.size());
        if (count) bytes.remove_prefix(count);
        if (std::ferror(stream_)) {
            if (errno == EINTR) {
                std::clearerr(stream_);
                continue;
            }
            io_error("Output: write");
        }
        if (count) continue;
        if (errno == EINTR) {
            std::clearerr(stream_);
            continue;
        }
        io_error("Output: write");
    }
}
void Output::flush()
{
    while (true) {
        errno = 0;
        if (detail::flush_stream(stream_) == 0) return;
        if (errno == EINTR) {
            std::clearerr(stream_);
            continue;
        }
        io_error("Output: flush");
    }
}
void flush() { Output{}.flush(); }

struct Reader::Impl {
    detail::Input input;
    Parser parser;
    ParserOptions options;
    std::array<char, 4096> bytes{};
    std::size_t begin = 0, end = 0;
    bool eof = false, terminal = false;
    std::optional<ReadResult> ready;
    std::optional<PendingTimeout> pending;
    std::optional<detail::Clock::time_point> origin, deadline;
    Impl(std::FILE* input, ParserOptions opts, InputProtocols protocols)
        : input{detail::descriptor(input)}, parser{opts, protocols}, options{opts}
    {
        if (!input) throw std::invalid_argument{"Reader: null stream"};
        if (this->input.descriptor < 0) io_error("Reader: descriptor");
        terminal = this->input.terminal;
#ifdef _WIN32
        this->input.protocols = protocols;
        parser.native_function_keys_ = terminal;
#endif
    }
    void update_deadline()
    {
        auto next = parser.pending_timeout();
        if (!next) {
            pending.reset();
            deadline.reset();
            origin.reset();
            return;
        }
        if (!origin) origin = detail::Clock::now();
        if (next != pending) {
            deadline = detail::deadline_after(*origin, *next == PendingTimeout::Escape
                                                           ? options.escape_timeout
                                                           : options.sequence_timeout);
            pending = next;
        }
    }
    auto read(std::optional<std::chrono::milliseconds> timeout) -> ReadResult
    {
        if (timeout && timeout->count() < 0)
            throw std::invalid_argument{"Reader: negative timeout"};
        // A zero-timeout call permits one readiness poll, not a timed wait.
        // Buffered events need no clock read; parser deadlines still use theirs.
        bool const polling = timeout && timeout->count() == 0;
        auto caller = timeout && !polling
                          ? detail::deadline_after(detail::Clock::now(), *timeout)
                          : detail::Clock::time_point::max();
        bool attempted_poll = false;
        while (true) {
            if (eof) {
                if (auto event = parser.finish()) return std::move(*event);
                return EndOfInput{};
            }
            auto parsed =
                parser.next(std::span<char const>{bytes.data() + begin, end - begin});
            begin += parsed.consumed;
            update_deadline();
            if (parsed.event) return std::move(*parsed.event);
            if (deadline && detail::Clock::now() >= *deadline) {
                auto event = parser.expire();
                update_deadline();
                if (event) return std::move(*event);
            }
            if (attempted_poll &&
                (polling || (caller != detail::Clock::time_point::max() &&
                             detail::Clock::now() >= caller)))
                return Timeout{};
            auto until = deadline ? std::min(caller, *deadline) : caller;
            attempted_poll = true;
            auto delay = polling ? 0
                         : until == detail::Clock::time_point::max()
                             ? -1
                             : detail::poll_delay(until);
            auto ready = detail::poll_input(input, delay);
            if (ready < 0) {
                if (errno == EINTR) continue;
                io_error("Reader: poll");
            }
            if (!ready) continue;
            std::ptrdiff_t count;
            do {
                count = detail::read_bytes(input, bytes.data(), bytes.size());
            } while (count < 0 && errno == EINTR && !polling &&
                     detail::Clock::now() < caller);
            if (count < 0) {
                if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) continue;
                io_error("Reader: read");
            }
            begin = 0;
            end = static_cast<std::size_t>(count);
            if (!count) {
                // VMIN=0 on a terminal means an empty readiness read is not EOF.
                if (!terminal || input.hung_up) eof = true;
            }
        }
    }
};
Reader::Reader(std::FILE* input, ParserOptions options, InputProtocols protocols)
    : impl_{std::make_unique<Impl>(input, options, protocols)}
{}
Reader::~Reader() = default;
Reader::Reader(Reader&&) noexcept = default;
auto Reader::operator=(Reader&&) noexcept -> Reader& = default;
auto Reader::read() -> ReadResult
{
    if (!impl_) throw std::logic_error{"Reader: moved from"};
    if (impl_->ready) {
        auto result = std::move(*impl_->ready);
        impl_->ready.reset();
        return result;
    }
    return impl_->read({});
}
auto Reader::read(std::chrono::milliseconds timeout) -> ReadResult
{
    if (!impl_) throw std::logic_error{"Reader: moved from"};
    if (timeout.count() < 0) throw std::invalid_argument{"Reader: negative timeout"};
    if (impl_->ready) {
        auto result = std::move(*impl_->ready);
        impl_->ready.reset();
        return result;
    }
    return impl_->read(timeout);
}
auto Reader::wait_state() -> WaitState
{
    if (!impl_) throw std::logic_error{"Reader: moved from"};
    if (!impl_->ready) {
        auto result = impl_->read(std::chrono::milliseconds{0});
        if (!std::holds_alternative<Timeout>(result)) impl_->ready = std::move(result);
    }
    bool ready = impl_->ready.has_value();
    if (!ready) {
        auto result = detail::poll_input(impl_->input, 0);
        if (result < 0 && errno != EINTR) io_error("Reader: wait readiness");
        // Partial units can consume a whole read-ahead block without producing
        // an event. Keep the host running while more bytes remain readable.
        ready = result != 0;
    }
    WaitState state{detail::wait_source(impl_->input), ready, impl_->deadline};
    if (state.source.kind == WaitSourceKind::None && !state.ready) {
        auto fallback =
            detail::deadline_after(detail::Clock::now(), std::chrono::milliseconds{2});
        if (!state.deadline || fallback < *state.deadline) state.deadline = fallback;
    }
    return state;
}
void Reader::configure(InputProtocols protocols)
{
    if (!impl_) throw std::logic_error{"Reader: moved from"};
    impl_->parser.configure(protocols);
#ifdef _WIN32
    impl_->input.protocols = protocols;
#endif
}
}  // namespace esc

namespace esc {
void Reader::prepare_queries()
{
    detail::prepare_queries(impl_->input);
#ifdef _WIN32
    // Preserve native records already normalized before switching transport.
#endif
}
void Reader::stop_queries() noexcept
{
    if (impl_) detail::stop_queries(impl_->input);
}
void Reader::configure_session(InputProtocols protocols,
                               MouseMode mode,
                               int output_descriptor)
{
    configure(protocols);
#ifdef _WIN32
    impl_->input.mouse_mode = mode;
    impl_->input.output_descriptor = output_descriptor;
#else
    (void)mode;
    (void)output_descriptor;
#endif
}
}  // namespace esc
