#include <array>
#include <cerrno>
#include <esc/terminal.hpp>
#include <limits>
#include <mutex>
#include <set>
#include "detail/backend.hpp"
#include "detail/protocol.hpp"

namespace esc {
namespace {
std::mutex lease_mutex;
std::set<std::uint64_t> leases;
auto error_code() -> std::error_code
{ return {errno ? errno : EIO, std::generic_category()}; }
void checked(int result, char const* message)
{
    if (result < 0) throw std::system_error{error_code(), message};
}
void require(bool supported, Feature feature)
{
    if (!supported) throw UnsupportedFeature{feature};
}
template <class T>
void valid_optional(std::optional<T> value, T first, T second)
{
    if (value && *value != first && *value != second)
        throw std::invalid_argument{"Terminal: invalid setting"};
}
void validate_options(TerminalOptions const& options)
{
    if (options.restore_modes.synchronized_output != SynchronizedOutput::Off &&
        options.restore_modes.synchronized_output != SynchronizedOutput::On)
        throw std::invalid_argument{"Terminal: invalid synchronization baseline"};
    valid_optional(options.echo, Echo::On, Echo::Off);
    valid_optional(options.input_buffer, InputBuffer::Canonical,
                   InputBuffer::Immediate);
    valid_optional(options.signals, Signals::On, Signals::Off);
    valid_optional(options.screen_buffer, ScreenBuffer::Normal,
                   ScreenBuffer::Alternate);
    valid_optional(options.cursor, CursorMode::Show, CursorMode::Hide);
    valid_optional(options.focus, FocusReporting::Off, FocusReporting::On);
    valid_optional(options.paste, BracketedPaste::Off, BracketedPaste::On);
    if (options.mouse && *options.mouse != MouseMode::Off &&
        *options.mouse != MouseMode::Basic && *options.mouse != MouseMode::Drag &&
        *options.mouse != MouseMode::Move)
        throw std::invalid_argument{"Terminal: invalid mouse mode"};
    if (options.resize_interval && options.resize_interval->count() <= 0)
        throw std::invalid_argument{"Terminal: nonpositive resize interval"};
}
void validate_restore(ProtocolState const& modes)
{
    detail::SequenceWriter measure;
    detail::encode(measure, modes.screen_buffer);
    detail::encode(measure, modes.cursor);
    detail::encode(measure, modes.mouse);
    detail::encode(measure, modes.focus);
    detail::encode(measure, modes.paste);
}
auto input_protocols(ProtocolState const& modes) -> InputProtocols
{
    return {
        modes.mouse.mode == MouseMode::Off ? MouseProtocol::None : modes.mouse.protocol,
        modes.focus == FocusReporting::On, modes.paste == BracketedPaste::On};
}
void validate_inherited(TerminalProfile const& profile, ProtocolState const& modes)
{
    if (modes.mouse.mode != MouseMode::Off) {
        require(modes.mouse.protocol == profile.mouse_protocol, Feature::Mouse);
        (void)detail::prepare(profile, modes.mouse);
    }
    if (modes.focus == FocusReporting::On)
        require(profile.focus_reporting, Feature::Focus);
    if (modes.paste == BracketedPaste::On)
        require(profile.bracketed_paste, Feature::BracketedPaste);
}
auto desired_modes(TerminalProfile const& profile,
                   ProtocolState modes,
                   TerminalOptions const& patch) -> ProtocolState
{
    if (patch.screen_buffer) modes.screen_buffer = *patch.screen_buffer;
    if (patch.cursor) modes.cursor = *patch.cursor;
    if (patch.mouse) {
        modes.mouse.mode = *patch.mouse;
        modes.mouse.protocol = *patch.mouse == MouseMode::Off ? MouseProtocol::None
                                                              : profile.mouse_protocol;
    }
    if (patch.focus) modes.focus = *patch.focus;
    if (patch.paste) modes.paste = *patch.paste;
    return modes;
}
auto changed_modes(ProtocolState const& before, ProtocolState const& after)
    -> std::array<bool, 5>
{
    // Off does not select a protocol, so harmless disabling is a no-op.
    bool mouse = before.mouse.mode != after.mouse.mode ||
                 (after.mouse.mode != MouseMode::Off &&
                  before.mouse.protocol != after.mouse.protocol);
    return {before.screen_buffer != after.screen_buffer, before.cursor != after.cursor,
            mouse, before.focus != after.focus, before.paste != after.paste};
}
void validate_transition(TerminalProfile const& profile,
                         ProtocolState const& before,
                         ProtocolState const& after)
{
    auto changed = changed_modes(before, after);
    if (changed[0]) require(profile.alternate_screen, Feature::AlternateScreen);
    if (changed[1]) require(profile.cursor_visibility, Feature::CursorVisibility);
    if (changed[2]) (void)detail::prepare(profile, after.mouse);
    if (changed[3]) require(profile.focus_reporting, Feature::Focus);
    if (changed[4]) require(profile.bracketed_paste, Feature::BracketedPaste);
}
void merge_os(TerminalOptions& current, TerminalOptions const& patch)
{
    if (patch.echo) current.echo = patch.echo;
    if (patch.input_buffer) current.input_buffer = patch.input_buffer;
    if (patch.signals) current.signals = patch.signals;
    if (patch.mouse) current.mouse = patch.mouse;
}
auto explicit_report(TerminalProfile profile) -> ProfileReport
{
    ProfileReport result{profile, {}};
    constexpr Feature features[] = {Feature::Control,
                                    Feature::Colors,
                                    Feature::Traits,
                                    Feature::AlternateScreen,
                                    Feature::CursorVisibility,
                                    Feature::Mouse,
                                    Feature::Focus,
                                    Feature::BracketedPaste,
                                    Feature::CursorStyle,
                                    Feature::CursorSaveRestore,
                                    Feature::Scrolling,
                                    Feature::Editing,
                                    Feature::WindowTitle,
                                    Feature::Hyperlinks,
                                    Feature::SynchronizedOutput};
    result.decisions.reserve(std::size(features));
    for (auto feature : features)
        result.decisions.push_back(
            {feature, EvidenceSource::Explicit, "Caller-supplied terminal profile"});
    return result;
}
}  // namespace
struct Frame::State {
    Output* output = nullptr;
    bool* usable = nullptr;
    bool open = true, baseline = false, touched = false, synchronized = false;
    unsigned depth = 0;
    std::optional<bool> confirmed;
    void command(SynchronizedOutput mode)
    {
        detail::send_encoded(*output, mode);
        output->flush();
    }
};
Frame::Frame(std::shared_ptr<State> const& state, bool synchronized) noexcept
    : state_{state}, synchronized_{synchronized}
{}
Frame::Frame(Frame&& other) noexcept
    : state_{std::move(other.state_)},
      active_{other.active_},
      synchronized_{other.synchronized_}
{ other.active_ = false; }
Frame::~Frame() noexcept
{
    try {
        close();
    }
    catch (...) {
    }
}
auto Frame::synchronized() const noexcept -> bool
{
    auto state = state_.lock();
    return active_ && state && state->open && synchronized_;
}
void Frame::close()
{
    if (!active_) return;
    active_ = false;
    auto state = state_.lock();
    if (!state || !state->open) return;
    if (--state->depth) return;
    try {
        if (state->touched) {
            state->command(SynchronizedOutput::Off);
            state->touched = false;
        }
        else
            state->output->flush();
    }
    catch (...) {
        *state->usable = false;
        throw;
    }
}

struct Terminal::Impl {
    Streams streams;
    int input_fd, output_fd;
    std::uint64_t device;
    ProfileReport report;
    TerminalOptions options;
    ProtocolState modes, restore;
    Output output;
    Reader reader;
    detail::Attributes captured{}, current{};
    Area last_size{};
    detail::Clock::time_point next_resize{};
    std::array<bool, 5> touched{};
    std::array<std::optional<Event>, 64> deferred;
    std::size_t deferred_head = 0, deferred_count = 0;
    bool replies = false, keyboard_touched = false;
    KeyboardProtocol keyboard = KeyboardProtocol::Legacy;
    unsigned saved_keyboard_flags = 0;
    void configure_reader()
    {
        auto protocols = input_protocols(modes);
        protocols.replies = replies;
        protocols.keyboard = keyboard;
        reader.configure_session(protocols, modes.mouse.mode, output_fd);
    }
    void enqueue(Event event) noexcept
    {
        static_assert(std::is_nothrow_move_constructible_v<Event>);
        deferred[(deferred_head + deferred_count) % deferred.size()].emplace(
            std::move(event));
        ++deferred_count;
    }
    auto dequeue() -> Event
    {
        auto result = std::move(*deferred[deferred_head]);
        deferred[deferred_head].reset();
        deferred_head = (deferred_head + 1) % deferred.size();
        --deferred_count;
        return result;
    }
    void keyboard_command(unsigned flags)
    {
        std::array<char, 32> bytes;
        detail::SequenceWriter writer{bytes.data()};
        writer.text("\x1b[=");
        writer.number(flags);
        writer.text(";1u");
        output.write(std::string_view{bytes.data(), writer.size()});
        output.flush();
    }
    bool leased = false, open = false, usable = true, os_touched = false;
    std::shared_ptr<Frame::State> frames = std::make_shared<Frame::State>();
    Impl(Streams s,
         int input,
         int out,
         std::uint64_t dev,
         ProfileReport r,
         TerminalOptions o,
         detail::Attributes attributes)
        : streams{s},
          input_fd{input},
          output_fd{out},
          device{dev},
          report{std::move(r)},
          options{o},
          modes{o.restore_modes},
          restore{o.restore_modes},
          output{s.output},
          reader{s.input, o.parser, input_protocols(o.restore_modes)},
          captured{attributes},
          current{attributes}
    {}
    ~Impl() noexcept
    {
        // Also covers unexpected exceptions while open() is still constructing.
        if (open) {
            try {
                cleanup();
            }
            catch (...) {
            }
        }
    }
    void reserve()
    {
        std::lock_guard guard{lease_mutex};
        if (!leases.insert(device).second)
            throw std::system_error{
                std::make_error_code(std::errc::device_or_resource_busy),
                "Terminal: device owned"};
        leased = true;
        open = true;
    }
    void release()
    {
        if (leased) {
            std::lock_guard guard{lease_mutex};
            leases.erase(device);
            leased = false;
        }
        open = false;
    }
    void emit(std::size_t index, ProtocolState const& target)
    {
        // Every mode command is bounded by 128 bytes, independently of a pack.
        std::array<char, 128> bytes;
        EncodeResult result{};
        switch (index) {
            case 0: result = escape_to(bytes, target.screen_buffer); break;
            case 1: result = escape_to(bytes, target.cursor); break;
            case 2: result = escape_to(bytes, target.mouse); break;
            case 3: result = escape_to(bytes, target.focus); break;
            case 4: result = escape_to(bytes, target.paste); break;
        }
        output.write(std::string_view{bytes.data(), result.written});
    }
    template <class Action>
    static void attempt(std::error_code& first, Action action)
    {
        try {
            action();
        }
        catch (std::system_error const& error) {
            if (!first) first = error.code();
        }
        catch (std::bad_alloc const&) {
            if (!first) first = std::make_error_code(std::errc::not_enough_memory);
        }
        catch (...) {
            if (!first) first = std::make_error_code(std::errc::io_error);
        }
    }
    auto restore_transition(ProtocolState const& old_modes,
                            detail::Attributes const& old_attributes,
                            std::array<bool, 5> const& attempted,
                            bool os) -> std::error_code
    {
        std::error_code error;
        for (std::size_t i = 5; i > 0; --i)
            if (attempted[i - 1]) attempt(error, [&] { emit(i - 1, old_modes); });
        attempt(error, [&] { output.flush(); });
        if (os)
            attempt(error, [&] {
                checked(detail::set_attributes(input_fd, &old_attributes),
                        "Terminal: rollback input");
            });
        return error;
    }
    void apply(TerminalOptions const& patch)
    {
        validate_options(patch);
        auto next = desired_modes(report.profile, modes, patch);
        validate_transition(report.profile, modes, next);
        auto new_options = options;
        merge_os(new_options, patch);
        auto attributes = detail::desired_attributes(captured, new_options);
        if (replies) attributes = detail::query_attributes(attributes);
        bool os = patch.echo.has_value() || patch.input_buffer.has_value() ||
                  patch.signals.has_value() || detail::transport_setup_required();
        auto changed = changed_modes(modes, next);
        if (frames->depth && changed[0])
            throw std::logic_error{
                "Terminal: close frames before changing screen buffer"};
        if (keyboard_touched && changed[0])
            throw std::logic_error{
                "Terminal: disable keyboard before changing screen buffer"};
        std::array<bool, 5> attempted{};
        if (os) {
            os_touched = true;
            if (detail::set_attributes(input_fd, &attributes) < 0) {
                auto primary = error_code();
                if (detail::set_attributes(input_fd, &current) < 0) {
                    usable = false;
                    throw SessionError{primary, error_code()};
                }
                throw std::system_error{primary, "Terminal: apply input"};
            }
        }
        try {
            for (std::size_t i = 0; i < 5; ++i)
                if (changed[i]) {
                    attempted[i] = true;
                    touched[i] = true;
                    emit(i, next);
                }
            output.flush();
        }
        catch (std::system_error const& primary) {
            usable = false;
            auto rollback = restore_transition(modes, current, attempted, os);
            throw SessionError{primary.code(), rollback};
        }
        catch (...) {
            // Even constructing an I/O exception can fail under memory pressure.
            usable = false;
            (void)restore_transition(modes, current, attempted, os);
            throw;
        }
        current = attributes;
        options.echo = new_options.echo;
        options.input_buffer = new_options.input_buffer;
        options.signals = new_options.signals;
        options.mouse = new_options.mouse;
        modes = next;
        configure_reader();
    }
    auto cleanup() -> std::error_code
    {
        std::error_code error;
        attempt(error, [&] { output.flush(); });
        if (frames->touched)
            attempt(error, [&] {
                frames->command(SynchronizedOutput::Off);
                frames->touched = false;
            });
        frames->open = false;
        frames->depth = 0;
        if (keyboard_touched)
            attempt(error, [&] { keyboard_command(saved_keyboard_flags); });
        for (std::size_t i = 5; i > 0; --i)
            if (touched[i - 1]) attempt(error, [&] { emit(i - 1, restore); });
        attempt(error, [&] { output.flush(); });
        reader.stop_queries();
        if (os_touched)
            attempt(error, [&] {
                checked(detail::set_attributes(input_fd, &captured),
                        "Terminal: restore input");
            });
        release();
        return error;
    }
};

auto TerminalOptions::interactive() -> TerminalOptions
{
    TerminalOptions result;
    result.echo = Echo::Off;
    result.input_buffer = InputBuffer::Immediate;
    result.screen_buffer = ScreenBuffer::Alternate;
    result.cursor = CursorMode::Hide;
    return result;
}
Terminal::Terminal(std::unique_ptr<Impl> implementation) noexcept
    : impl_{std::move(implementation)}
{}
Terminal::Terminal(Terminal&&) noexcept = default;
Terminal::~Terminal() noexcept
{
    try {
        close();
    }
    catch (...) {
    }
}
auto Terminal::open(TerminalOptions options) -> Terminal
{ return open(Streams{}, options); }
auto Terminal::open(Streams streams, TerminalOptions options) -> Terminal
{
    if (!streams.input || !streams.output)
        throw std::invalid_argument{"Terminal: null stream"};
    validate_options(options);
    validate_restore(options.restore_modes);
    int input = detail::descriptor(streams.input),
        output = detail::descriptor(streams.output);
    auto device = detail::terminal_identity(input, output);
    auto report = options.profile ? explicit_report(*options.profile)
                                  : detail::session_profile(input, output);
    detail::validate_profile(report.profile);
    validate_inherited(report.profile, options.restore_modes);
    auto target = desired_modes(report.profile, options.restore_modes, options);
    validate_transition(report.profile, options.restore_modes, target);
    // Allocate parser/session storage and validate limits before OS changes.
    auto impl =
        std::make_unique<Impl>(streams, input, output, device, std::move(report),
                               options, detail::Attributes{});
    checked(detail::get_attributes(input, output, &impl->captured),
            "Terminal: capture input");
    impl->current = impl->captured;
    (void)detail::desired_attributes(impl->captured, options);
    impl->last_size = detail::get_size(output);
    impl->next_resize = detail::deadline_after(
        detail::Clock::now(),
        options.resize_interval.value_or(std::chrono::milliseconds{100}));
    impl->reserve();
    try {
        if (detail::transport_setup_required()) {
            impl->output.flush();
            impl->os_touched = true;
            detail::begin_session(impl->captured);
            impl->current = impl->captured;
        }
        impl->apply(options);
    }
    catch (SessionError const& primary) {
        auto cleanup = impl->cleanup();
        throw SessionError{primary.code(), primary.rollback_error()
                                               ? primary.rollback_error()
                                               : cleanup};
    }
    catch (std::system_error const& primary) {
        auto cleanup = impl->cleanup();
        if (cleanup) throw SessionError{primary.code(), cleanup};
        throw;
    }
    return Terminal{std::move(impl)};
}
void Terminal::require_open() const
{
    if (!impl_ || !impl_->open || !impl_->usable)
        throw std::logic_error{"Terminal: session is closed or unusable"};
}
auto Terminal::is_open() const noexcept -> bool { return impl_ && impl_->open; }
auto Terminal::profile() const -> TerminalProfile const&
{
    require_open();
    return impl_->report.profile;
}
auto Terminal::profile_report() const -> ProfileReport const&
{
    require_open();
    return impl_->report;
}
auto Terminal::output() -> Output&
{
    require_open();
    return impl_->output;
}
void Terminal::flush() { output().flush(); }
void Terminal::apply(TerminalOptions const& patch)
{
    require_open();
    impl_->apply(patch);
}
auto Terminal::area() const -> Area
{
    require_open();
    return detail::get_size(impl_->output_fd);
}
void Terminal::close()
{
    if (!is_open()) return;
    auto error = impl_->cleanup();
    if (error) throw std::system_error{error, "Terminal: cleanup"};
}
auto Terminal::frame() -> Frame
{
    require_open();
    auto& state = *impl_->frames;
    if (state.depth == (std::numeric_limits<unsigned>::max)())
        throw std::length_error{"Frame: nesting overflow"};
    if (!state.depth) {
        state.output = &impl_->output;
        state.usable = &impl_->usable;
        state.synchronized =
            state.confirmed.value_or(impl_->report.profile.synchronized_output);
        if (!state.synchronized &&
            impl_->report.profile.fallback == FallbackPolicy::Strict)
            throw UnsupportedFeature{Feature::SynchronizedOutput};
        if (!state.confirmed)
            state.baseline =
                impl_->restore.synchronized_output == SynchronizedOutput::On;
        impl_->output.flush();
        if (state.synchronized && !state.baseline) {
            state.touched = true;
            try {
                state.command(SynchronizedOutput::On);
            }
            catch (std::system_error const& primary) {
                std::error_code rollback;
                Impl::attempt(rollback, [&] {
                    state.command(SynchronizedOutput::Off);
                    state.touched = false;
                });
                impl_->usable = false;
                throw SessionError{primary.code(), rollback};
            }
            catch (...) {
                std::error_code rollback;
                Impl::attempt(rollback, [&] {
                    state.command(SynchronizedOutput::Off);
                    state.touched = false;
                });
                impl_->usable = false;
                throw;
            }
        }
    }
    ++state.depth;
    return Frame{impl_->frames, state.synchronized};
}
auto Terminal::confirm_synchronized_output(std::chrono::milliseconds timeout)
    -> QueryResult
{
    require_open();
    if (impl_->frames->depth)
        throw std::logic_error{"Terminal: cannot probe synchronization inside a frame"};
    auto result = query(ModeQuery{2026}, timeout);
    if (result.status == QueryStatus::Reply) {
        auto mode = std::get<ModeReply>(*result.reply).status;
        bool supported = mode == ModeStatus::Set || mode == ModeStatus::Reset;
        impl_->frames->confirmed = supported;
        impl_->frames->baseline = mode == ModeStatus::Set;
        if (!supported) result.status = QueryStatus::Rejected;
    }
    return result;
}
auto Terminal::wait_state() -> WaitState
{
    require_open();
    if (impl_->deferred_count) return {{}, true, {}};
    auto state = impl_->reader.wait_state();
    if (!impl_->options.resize_interval) return state;
    auto now = detail::Clock::now();
    if (!state.ready && now >= impl_->next_resize) {
        auto size = area();
        impl_->next_resize =
            detail::deadline_after(now, *impl_->options.resize_interval);
        if (size != impl_->last_size) {
            impl_->last_size = size;
            impl_->enqueue(Resize{size});
            state.ready = true;
        }
    }
    if (!state.deadline || impl_->next_resize < *state.deadline)
        state.deadline = impl_->next_resize;
    return state;
}
auto Terminal::read() -> ReadResult { return read_until({}); }
auto Terminal::read(std::chrono::milliseconds timeout) -> ReadResult
{ return read_until(timeout); }
auto Terminal::read_until(std::optional<std::chrono::milliseconds> timeout)
    -> ReadResult
{
    require_open();
    if (timeout && timeout->count() < 0)
        throw std::invalid_argument{"Terminal: negative timeout"};
    if (impl_->deferred_count) return {ReadStatus::Event, impl_->dequeue()};
    auto deadline = timeout ? detail::deadline_after(detail::Clock::now(), *timeout)
                            : detail::Clock::time_point::max();
    while (true) {
        auto pending = impl_->reader.read(std::chrono::milliseconds{0});
        if (pending.status != ReadStatus::Timeout) return pending;
        auto now = detail::Clock::now();
        if (impl_->options.resize_interval && now >= impl_->next_resize) {
            auto size = area();
            impl_->next_resize =
                detail::deadline_after(now, *impl_->options.resize_interval);
            if (size != impl_->last_size) {
                impl_->last_size = size;
                return {ReadStatus::Event, Resize{size}};
            }
        }
        if (now >= deadline) return {ReadStatus::Timeout, {}};
        auto until = impl_->options.resize_interval
                         ? std::min(deadline, impl_->next_resize)
                         : deadline;
        if (until == detail::Clock::time_point::max()) return impl_->reader.read();
        auto wait = std::chrono::ceil<std::chrono::milliseconds>(until - now);
        auto result = impl_->reader.read(wait);
        if (result.status != ReadStatus::Timeout) return result;
    }
}
namespace {
auto matches(Query const& query, Reply const& reply) -> bool
{
    return std::visit(
        [&](auto const& request) {
            using T = std::decay_t<decltype(request)>;
            if constexpr (std::same_as<T, DeviceAttributesQuery>)
                return std::holds_alternative<DeviceAttributesReply>(reply);
            else if constexpr (std::same_as<T, KeyboardQuery>)
                return std::holds_alternative<KeyboardReply>(reply);
            else {
                auto mode = std::get_if<ModeReply>(&reply);
                return mode && mode->mode == request.mode;
            }
        },
        query);
}
auto remaining(detail::Clock::time_point deadline) -> std::chrono::milliseconds
{
    auto now = detail::Clock::now();
    return now >= deadline
               ? std::chrono::milliseconds{0}
               : std::chrono::ceil<std::chrono::milliseconds>(deadline - now);
}
}  // namespace
void Terminal::request(Query const& query)
{ request(std::span<Query const>{&query, 1}); }
void Terminal::request(std::span<Query const> queries)
{
    require_open();
    if (queries.empty()) return;
    require(impl_->report.profile.control, Feature::Control);
    // Validate the entire pack before changing transport or emitting anything.
    for (auto const& query : queries)
        (void)query_to({}, query);
    if (!impl_->replies) {
        auto attributes = detail::query_attributes(impl_->current);
        impl_->reader.prepare_queries();  // Allocate transport before OS changes.
        if (detail::transport_setup_required()) {
            impl_->os_touched = true;
            if (detail::set_attributes(impl_->input_fd, &attributes) < 0) {
                auto primary = error_code();
                impl_->reader.stop_queries();
                if (detail::set_attributes(impl_->input_fd, &impl_->current) < 0) {
                    impl_->usable = false;
                    throw SessionError{primary, error_code()};
                }
                throw std::system_error{primary, "Query: enable input transport"};
            }
            impl_->current = attributes;
        }
        impl_->replies = true;
        impl_->configure_reader();
    }
    std::array<char, 256> batch;
    std::size_t used = 0;
    for (auto const& query : queries) {
        auto required = query_to({}, query).required;
        if (required > batch.size() - used) {
            impl_->output.write(std::string_view{batch.data(), used});
            used = 0;
        }
        used += query_to(std::span<char>{batch}.subspan(used), query).written;
    }
    impl_->output.write(std::string_view{batch.data(), used});
    impl_->output.flush();
}
auto Terminal::query(Query const& request, std::chrono::milliseconds timeout)
    -> QueryResult
{
    require_open();
    if (timeout.count() < 0) throw std::invalid_argument{"Query: negative timeout"};
    // Validate even when the preservation queue is full.
    std::array<char, 32> validation;
    (void)query_to(validation, request);
    require(impl_->report.profile.control, Feature::Control);
    if (impl_->deferred_count == impl_->deferred.size())
        return {QueryStatus::InputPending, {}};
    auto deadline = detail::deadline_after(detail::Clock::now(), timeout);
    this->request(request);
    bool attempted = false;
    while (true) {
        if (impl_->deferred_count == impl_->deferred.size())
            return {QueryStatus::InputPending, {}};
        if (attempted && detail::Clock::now() >= deadline)
            return {QueryStatus::Timeout, {}};
        attempted = true;
        auto result = impl_->reader.read(remaining(deadline));
        if (result.status == ReadStatus::Timeout) return {QueryStatus::Timeout, {}};
        if (result.status == ReadStatus::EndOfInput)
            return {QueryStatus::EndOfInput, {}};
        if (auto reply = std::get_if<TerminalReply>(&*result.event)) {
            if (matches(request, reply->value))
                return {QueryStatus::Reply, std::move(reply->value)};
        }
        impl_->enqueue(std::move(*result.event));
    }
}
auto Terminal::probe(std::chrono::milliseconds timeout) -> ProbeReport
{
    require_open();
    if (timeout.count() < 0) throw std::invalid_argument{"Probe: negative timeout"};
    auto deadline = detail::deadline_after(detail::Clock::now(), timeout);
    ProbeReport report;
    Query const requests[] = {KeyboardQuery{}, ModeQuery{25},   ModeQuery{1049},
                              ModeQuery{1000}, ModeQuery{1002}, ModeQuery{1003},
                              ModeQuery{1006}, ModeQuery{1004}, ModeQuery{2004},
                              ModeQuery{2026}};
    report.observations.reserve(std::size(requests));
    for (auto const& request : requests)
        report.observations.push_back(
            {request, {QueryStatus::Timeout, {}}, EvidenceSource::Probe});
    if (impl_->deferred_count == impl_->deferred.size()) {
        for (auto& observation : report.observations)
            observation.result.status = QueryStatus::InputPending;
        return report;
    }
    request(std::span<Query const>{requests});
    std::size_t waiting = std::size(requests);
    bool attempted = false;
    while (waiting) {
        QueryStatus stop = QueryStatus::Timeout;
        if (impl_->deferred_count == impl_->deferred.size())
            stop = QueryStatus::InputPending;
        else if (!attempted || detail::Clock::now() < deadline) {
            attempted = true;
            auto result = impl_->reader.read(remaining(deadline));
            if (result.status == ReadStatus::Event) {
                bool matched = false;
                if (auto reply = std::get_if<TerminalReply>(&*result.event))
                    for (auto& observation : report.observations)
                        if (observation.result.status != QueryStatus::Reply &&
                            matches(observation.request, reply->value)) {
                            observation.result = {QueryStatus::Reply,
                                                  std::move(reply->value)};
                            --waiting;
                            matched = true;
                            break;
                        }
                if (!matched) impl_->enqueue(std::move(*result.event));
                continue;
            }
            if (result.status == ReadStatus::EndOfInput) stop = QueryStatus::EndOfInput;
        }
        for (auto& observation : report.observations)
            if (observation.result.status != QueryStatus::Reply)
                observation.result.status = stop;
        break;
    }
    return report;
}

auto Terminal::keyboard_protocol() const -> KeyboardProtocol
{
    require_open();
    return impl_->keyboard;
}
void Terminal::disable_keyboard()
{
    require_open();
    if (!impl_->keyboard_touched) return;
    try {
        impl_->keyboard_command(impl_->saved_keyboard_flags);
    }
    catch (...) {
        impl_->usable = false;
        throw;
    }
    impl_->keyboard_touched = false;
    impl_->keyboard = impl_->saved_keyboard_flags ? KeyboardProtocol::Kitty
                                                  : KeyboardProtocol::Legacy;
    impl_->configure_reader();
}
auto Terminal::enable_keyboard(KittyFlags flags, std::chrono::milliseconds timeout)
    -> QueryResult
{
    require_open();
    detail::validate_kitty_flags(flags);
    if (timeout.count() < 0) throw std::invalid_argument{"Kitty: negative timeout"};
    auto deadline = detail::deadline_after(detail::Clock::now(), timeout);
    auto captured = query(KeyboardQuery{}, remaining(deadline));
    if (captured.status != QueryStatus::Reply) return captured;
    unsigned previous = std::get<KeyboardReply>(*captured.reply).flags;
    auto old_protocol = previous ? KeyboardProtocol::Kitty : KeyboardProtocol::Legacy;
    bool old_touched = impl_->keyboard_touched;
    if (!impl_->keyboard_touched) impl_->saved_keyboard_flags = previous;
    impl_->keyboard_touched = true;  // Cleanup covers partial writes too.
    impl_->keyboard = KeyboardProtocol::Kitty;
    impl_->configure_reader();
    try {
        impl_->keyboard_command(static_cast<unsigned>(flags));
        auto accepted = query(KeyboardQuery{}, remaining(deadline));
        if (accepted.status == QueryStatus::Reply &&
            std::get<KeyboardReply>(*accepted.reply).flags ==
                static_cast<unsigned>(flags))
            return accepted;
        impl_->keyboard_command(previous);
        impl_->keyboard = old_protocol;
        impl_->keyboard_touched = old_touched;
        impl_->configure_reader();
        if (accepted.status == QueryStatus::Reply)
            accepted.status = QueryStatus::Rejected;
        return accepted;
    }
    catch (std::system_error const& primary) {
        std::error_code rollback;
        Impl::attempt(rollback, [&] { impl_->keyboard_command(previous); });
        impl_->usable = false;
        throw SessionError{primary.code(), rollback};
    }
    catch (...) {
        std::error_code rollback;
        Impl::attempt(rollback, [&] { impl_->keyboard_command(previous); });
        impl_->usable = false;
        throw;
    }
}
}  // namespace esc
