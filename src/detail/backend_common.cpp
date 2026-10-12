#include <algorithm>
#include <cerrno>
#include <climits>
#include <system_error>
#include "backend.hpp"
#ifdef ESCAPE_CONTRACT_TEST_HOOKS
#    include <array>
#    include <thread>
#    include <vector>
#    include "faults.hpp"
namespace esc::testing {
struct FaultScope::Impl {
    std::vector<Fault> faults;
    std::array<std::size_t, 8> calls{}, bytes{};
    std::vector<bool> fired;
    Impl* previous = nullptr;
};
static thread_local FaultScope::Impl* active = nullptr;
FaultScope::FaultScope(std::initializer_list<Fault> faults)
    : implementation_{std::make_unique<Impl>()}
{
    implementation_->faults = faults;
    implementation_->fired.resize(faults.size());
    implementation_->previous = active;
    active = implementation_.get();
}
FaultScope::~FaultScope() { active = implementation_->previous; }
auto FaultScope::calls(Operation operation) const -> std::size_t
{ return implementation_->calls[static_cast<unsigned>(operation)]; }
// Private friend-free adapter accesses only this translation unit's scope.
auto intercept(Operation operation, std::size_t count) -> std::pair<bool, std::size_t>
{
    if (!active) return {false, count};
    auto index = static_cast<unsigned>(operation);
    auto call = ++active->calls[index];
    for (std::size_t i = 0; i < active->faults.size(); ++i) {
        auto const& fault = active->faults[i];
        if (fault.operation != operation || (active->fired[i] && !fault.persistent))
            continue;
        bool by_bytes = operation == Operation::Write &&
                        fault.after_bytes != std::numeric_limits<std::size_t>::max();
        bool due = by_bytes ? active->bytes[index] >= fault.after_bytes
                            : call >= fault.on_call;
        if (due && fault.error) {
            if (fault.delay.count()) std::this_thread::sleep_for(fault.delay);
            active->fired[i] = true;
            errno = fault.error.value();
            return {true, 0};
        }
        if (call >= fault.on_call) count = std::min(count, fault.maximum_transfer);
        if (by_bytes) count = std::min(count, fault.after_bytes - active->bytes[index]);
    }
    return {false, count};
}
void transferred(Operation operation, std::size_t count)
{
    if (active) active->bytes[static_cast<unsigned>(operation)] += count;
}
}  // namespace esc::testing
#endif
namespace esc::detail {
#ifdef ESCAPE_CONTRACT_TEST_HOOKS
using testing::Operation;
#endif
auto write_bytes(std::FILE* stream, char const* bytes, std::size_t count) -> std::size_t
{
    auto [fail, limit] = ESC_INTERCEPT(Write, count);
    if (fail) return 0;
    auto written = std::fwrite(bytes, 1, limit, stream);
#ifdef ESCAPE_CONTRACT_TEST_HOOKS
    testing::transferred(Operation::Write, written);
#endif
    return written;
}
auto flush_stream(std::FILE* stream) -> int
{
    auto [fail, limit] = ESC_INTERCEPT(Flush, 0);
    (void)limit;
    if (fail) return EOF;
    return std::fflush(stream);
}
auto environment() -> Environment
{
    auto [fail, limit] = ESC_INTERCEPT(Environment, 0);
    (void)limit;
    if (fail)
        throw std::system_error{errno, std::generic_category(),
                                "Terminal: environment"};
    return snapshot_environment();
}
auto deadline_after(Clock::time_point start, std::chrono::milliseconds duration)
    -> Clock::time_point
{
    auto available = std::chrono::duration_cast<std::chrono::milliseconds>(
        Clock::time_point::max() - start);
    return duration >= available ? Clock::time_point::max() : start + duration;
}
auto poll_delay(Clock::time_point deadline) -> int
{
    auto remaining = deadline - Clock::now();
    if (remaining <= Clock::duration::zero()) return 0;
    auto delay = std::chrono::ceil<std::chrono::milliseconds>(remaining).count();
    return static_cast<int>(std::min<decltype(delay)>(delay, INT_MAX));
}
}  // namespace esc::detail
