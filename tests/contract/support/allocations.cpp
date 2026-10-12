#include "allocations.hpp"

#include <cstdlib>
#include <new>
#ifdef _WIN32
#    include <malloc.h>
#endif

namespace {
void note()
{
    if (contract::track_allocations) ++contract::allocation_count;
    if (contract::reject_allocations) throw std::bad_alloc{};
}

auto allocate(std::size_t size) -> void*
{
    note();
    if (auto* result = std::malloc(size ? size : 1)) return result;
    throw std::bad_alloc{};
}

auto allocate_aligned(std::size_t size, std::size_t alignment) -> void*
{
    note();
#ifdef _WIN32
    if (auto* result = ::_aligned_malloc(size ? size : 1, alignment)) return result;
#else
    void* result = nullptr;
    if (::posix_memalign(&result, alignment, size ? size : 1) == 0) return result;
#endif
    throw std::bad_alloc{};
}

void free_aligned(void* p) noexcept
{
#ifdef _WIN32
    ::_aligned_free(p);
#else
    std::free(p);
#endif
}
}  // namespace

void* operator new(std::size_t n) { return allocate(n); }

void* operator new[](std::size_t n) { return allocate(n); }

void operator delete(void* p) noexcept { std::free(p); }

void operator delete[](void* p) noexcept { std::free(p); }

void operator delete(void* p, std::size_t) noexcept { std::free(p); }

void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

void* operator new(std::size_t n, std::align_val_t alignment)
{ return allocate_aligned(n, static_cast<std::size_t>(alignment)); }

void* operator new[](std::size_t n, std::align_val_t alignment)
{ return allocate_aligned(n, static_cast<std::size_t>(alignment)); }

void operator delete(void* p, std::align_val_t) noexcept { free_aligned(p); }

void operator delete[](void* p, std::align_val_t) noexcept { free_aligned(p); }

void operator delete(void* p, std::size_t, std::align_val_t) noexcept
{ free_aligned(p); }

void operator delete[](void* p, std::size_t, std::align_val_t) noexcept
{ free_aligned(p); }

void* operator new(std::size_t n, std::nothrow_t const&) noexcept
{
    try {
        return allocate(n);
    }
    catch (...) {
        return nullptr;
    }
}

void* operator new[](std::size_t n, std::nothrow_t const&) noexcept
{
    try {
        return allocate(n);
    }
    catch (...) {
        return nullptr;
    }
}

void operator delete(void* p, std::nothrow_t const&) noexcept { std::free(p); }

void operator delete[](void* p, std::nothrow_t const&) noexcept { std::free(p); }

void* operator new(std::size_t n, std::align_val_t a, std::nothrow_t const&) noexcept
{
    try {
        return allocate_aligned(n, static_cast<std::size_t>(a));
    }
    catch (...) {
        return nullptr;
    }
}

void* operator new[](std::size_t n, std::align_val_t a, std::nothrow_t const&) noexcept
{
    try {
        return allocate_aligned(n, static_cast<std::size_t>(a));
    }
    catch (...) {
        return nullptr;
    }
}

void operator delete(void* p, std::align_val_t, std::nothrow_t const&) noexcept
{ free_aligned(p); }

void operator delete[](void* p, std::align_val_t, std::nothrow_t const&) noexcept
{ free_aligned(p); }
