#pragma once

#include "thread_cache.hpp"

#include <atomic>
#include <cstdint>

namespace mae {

// ---------------------------------------------------------------------------
// OS chunk sizing (used when the central pool runs dry)
// ---------------------------------------------------------------------------

inline constexpr std::size_t DEFAULT_CHUNK_DATA_BYTES = 65536;

[[nodiscard]] inline constexpr std::size_t chunk_header_size() noexcept
{
    return offsetof(Chunk, data);
}

[[nodiscard]] inline constexpr std::size_t chunk_total_size(
    std::size_t data_bytes) noexcept
{
    return chunk_header_size() + data_bytes;
}

// ---------------------------------------------------------------------------
// 128-bit tagged head for ABA-safe lock-free CAS (requires 16-byte atomics)
// ---------------------------------------------------------------------------

struct alignas(16) TaggedHead {
    MemoryCell* ptr{nullptr};
    std::uintptr_t tag{0};

    [[nodiscard]] friend bool operator==(
        const TaggedHead& lhs,
        const TaggedHead& rhs) noexcept
    {
        return lhs.ptr == rhs.ptr && lhs.tag == rhs.tag;
    }

    [[nodiscard]] friend bool operator!=(
        const TaggedHead& lhs,
        const TaggedHead& rhs) noexcept
    {
        return !(lhs == rhs);
    }
};

static_assert(
    alignof(TaggedHead) == 16,
    "TaggedHead must be 16-byte aligned for cmpxchg16b");

//static_assert(
    //std::atomic<TaggedHead>::is_always_lock_free,
    //"Platform must provide lock-free 16-byte CAS (-mcx16 on GCC/Clang x86-64)");

// ---------------------------------------------------------------------------
// Per-size-class lock-free list head (one cache line each)
// ---------------------------------------------------------------------------

struct SizeClassCentralList {
    std::atomic<TaggedHead> head{};

    std::byte _padding
        [(CACHE_LINE_SIZE - (sizeof(std::atomic<TaggedHead>)) % CACHE_LINE_SIZE)
         % CACHE_LINE_SIZE];
};

static_assert(
    sizeof(SizeClassCentralList) == CACHE_LINE_SIZE,
    "Each central list head must occupy exactly one cache line");

// ---------------------------------------------------------------------------
// Global central pool singleton
// ---------------------------------------------------------------------------

struct alignas(CACHE_LINE_SIZE) CentralPool {
    SizeClassCentralList lists[SIZE_CLASS_COUNT];
};

static_assert(alignof(CentralPool) == CACHE_LINE_SIZE, "CentralPool must be cache-line aligned");

CentralPool& central_pool() noexcept;

// ---------------------------------------------------------------------------
// Lock-free central batch transfer (internal)
// ---------------------------------------------------------------------------

MemoryCell* pop_batch_from_central(
    std::size_t class_index,
    std::size_t batch_size) noexcept;

void push_batch_to_central(
    MemoryCell* batch_head,
    MemoryCell* batch_tail,
    std::size_t class_index) noexcept;

// ---------------------------------------------------------------------------
// Chunk replenishment (allocates a new Slab-backed Chunk from the OS)
// ---------------------------------------------------------------------------

bool replenish_central_pool(std::size_t class_index) noexcept;

} // namespace mae
