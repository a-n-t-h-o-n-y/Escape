#pragma once

#include <cstddef>
#include <new>

namespace contract {
// Thread-local, so setup/other threads do not pollute an operation's allocation count.
inline thread_local bool track_allocations = false;
inline thread_local bool reject_allocations = false;
inline thread_local std::size_t allocation_count = 0;

struct AllocationScope {
    bool previous_track = track_allocations;
    bool previous_reject = reject_allocations;
    std::size_t previous_count = allocation_count;

    explicit AllocationScope(bool reject = false)
    {
        allocation_count = 0;
        track_allocations = true;
        reject_allocations = reject;
    }

    auto count() const noexcept -> std::size_t { return allocation_count; }

    ~AllocationScope()
    {
        track_allocations = previous_track;
        reject_allocations = previous_reject;
        allocation_count = previous_count;
    }
};
}  // namespace contract
