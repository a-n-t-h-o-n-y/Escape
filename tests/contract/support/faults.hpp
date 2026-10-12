#pragma once

// Test-only seam contract. Definitions belong in the future backend test adapter.
// This deliberately does NOT fake successful I/O or provide a no-op implementation.
// Production public headers must not include this file.

#include <chrono>
#include <cstddef>
#include <initializer_list>
#include <limits>
#include <memory>
#include <system_error>

namespace esc::testing {

enum class Operation {
    Read,
    Poll,
    Write,
    Flush,
    GetAttributes,
    SetAttributes,
    GetSize,
    Environment
};

struct Fault {
    Operation operation;
    std::size_t on_call = 1;
    std::error_code error = {};
    std::size_t maximum_transfer = std::numeric_limits<std::size_t>::max();
    // For Write: when finite this selects byte-based failure instead of on_call.
    // Accept this many bytes across calls, then fail once, including splitting
    // a transfer so tests do not depend on the library's batching.
    std::size_t after_bytes = std::numeric_limits<std::size_t>::max();
    bool persistent = false;
    std::chrono::milliseconds delay{0};
};

class FaultScope {
   public:
    explicit FaultScope(std::initializer_list<Fault> faults = {});

    ~FaultScope();

    FaultScope(FaultScope const&) = delete;

    auto operator=(FaultScope const&) -> FaultScope& = delete;

    auto calls(Operation) const -> std::size_t;

   public:
    struct Impl;

   private:
    std::unique_ptr<Impl> implementation_;
};
}  // namespace esc::testing
