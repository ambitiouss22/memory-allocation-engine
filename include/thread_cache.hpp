#pragma once

#include "slab_allocator.hpp"

namespace mae {

// ---------------------------------------------------------------------------
// Size classes (allocation buckets up to 256 bytes)
// ---------------------------------------------------------------------------

inline constexpr std::size_t SIZE_CLASS_COUNT = 5;
inline constexpr std::size_t MAX_SIZE_CLASS_BYTES = 256;

inline constexpr std::size_t SIZE_CLASS_BYTES[SIZE_CLASS_COUNT] = {
    16, 32, 64, 128, 256,
};

inline constexpr std::size_t CENTRAL_FALLBACK_BATCH = 8;

[[nodiscard]] inline constexpr std::size_t size_to_class_index(
    std::size_t size) noexcept
{
    if (size <= SIZE_CLASS_BYTES[0]) {
        return 0;
    }
    if (size <= SIZE_CLASS_BYTES[1]) {
        return 1;
    }
    if (size <= SIZE_CLASS_BYTES[2]) {
        return 2;
    }
    if (size <= SIZE_CLASS_BYTES[3]) {
        return 3;
    }
    if (size <= SIZE_CLASS_BYTES[4]) {
        return 4;
    }
    return SIZE_CLASS_COUNT;
}

[[nodiscard]] inline constexpr bool is_size_class_allocatable(
    std::size_t size) noexcept
{
    return size_to_class_index(size) < SIZE_CLASS_COUNT;
}

[[nodiscard]] inline constexpr std::size_t size_class_bytes(
    std::size_t class_index) noexcept
{
    return SIZE_CLASS_BYTES[class_index];
}

// Actual footprint for central / aligned_alloc (size must be a multiple of alignment).
[[nodiscard]] inline constexpr std::size_t size_class_allocation_bytes(
    std::size_t class_index) noexcept
{
    return align_up(SIZE_CLASS_BYTES[class_index], CACHE_LINE_SIZE);
}

// ---------------------------------------------------------------------------
// Per-thread cache (strictly single-threaded; no locks or atomics)
// ---------------------------------------------------------------------------

struct alignas(CACHE_LINE_SIZE) ThreadCache {
    MemoryCell* free_lists[SIZE_CLASS_COUNT];

    std::byte _padding
        [(CACHE_LINE_SIZE - (SIZE_CLASS_COUNT * sizeof(MemoryCell*)) % CACHE_LINE_SIZE)
         % CACHE_LINE_SIZE];
};

static_assert(
    sizeof(ThreadCache) % CACHE_LINE_SIZE == 0,
    "ThreadCache must occupy whole cache lines");

static_assert(alignof(ThreadCache) == CACHE_LINE_SIZE, "ThreadCache must be cache-line aligned");

// ---------------------------------------------------------------------------
// Thread-local instance and fast-path interface
// ---------------------------------------------------------------------------

ThreadCache& thread_cache() noexcept;

void* allocate_local(std::size_t size) noexcept;
void deallocate_local(void* ptr, std::size_t size) noexcept;

// Refills the thread-local cache from the lock-free central pool.
MemoryCell* fetch_batch_from_central(std::size_t class_index) noexcept;

} // namespace mae
