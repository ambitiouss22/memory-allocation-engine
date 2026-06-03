#pragma once

#include <cstddef>
#include <cstdint>

namespace mae {

// ---------------------------------------------------------------------------
// Core constants
// ---------------------------------------------------------------------------

inline constexpr std::size_t CACHE_LINE_SIZE = 64;
inline constexpr std::size_t CACHE_LINE_MASK = CACHE_LINE_SIZE - 1;

// Default raw storage capacity embedded in each Slab (excluding header).
inline constexpr std::size_t DEFAULT_SLAB_STORAGE_BYTES = 4096;

// ---------------------------------------------------------------------------
// Alignment helpers
// ---------------------------------------------------------------------------

[[nodiscard]] inline constexpr std::size_t align_up(
    std::size_t value,
    std::size_t alignment = CACHE_LINE_SIZE) noexcept
{
    return (value + alignment - 1) & ~(alignment - 1);
}

[[nodiscard]] inline constexpr bool is_aligned(
    std::size_t value,
    std::size_t alignment = CACHE_LINE_SIZE) noexcept
{
    return (value & (alignment - 1)) == 0;
}

[[nodiscard]] inline constexpr std::size_t pointer_to_integer(const void* ptr) noexcept
{
    return static_cast<std::size_t>(reinterpret_cast<std::uintptr_t>(ptr));
}

[[nodiscard]] inline constexpr bool is_pointer_aligned(
    const void* ptr,
    std::size_t alignment = CACHE_LINE_SIZE) noexcept
{
    return is_aligned(pointer_to_integer(ptr), alignment);
}

// ---------------------------------------------------------------------------
// Intrusive free-list cell
// ---------------------------------------------------------------------------

struct MemoryCell {
    MemoryCell* next;
};

// ---------------------------------------------------------------------------
// Slab: fixed-size object pool backed by a cache-line-aligned byte buffer
// ---------------------------------------------------------------------------

struct alignas(CACHE_LINE_SIZE) Slab {
    std::size_t object_size;
    std::size_t cell_count;
    std::size_t storage_bytes;
    MemoryCell* free_list_head;

    // Explicit padding so `storage` begins on a 64-byte boundary.
    std::byte _header_padding
        [(CACHE_LINE_SIZE
          - ((sizeof(std::size_t) * 3 + sizeof(MemoryCell*)) % CACHE_LINE_SIZE))
         % CACHE_LINE_SIZE];

    alignas(CACHE_LINE_SIZE) std::byte storage[1];
};

static_assert(
    offsetof(Slab, storage) % CACHE_LINE_SIZE == 0,
    "Slab::storage must begin on a cache-line boundary");

static_assert(alignof(Slab) == CACHE_LINE_SIZE, "Slab must be cache-line aligned");

// ---------------------------------------------------------------------------
// Chunk: contiguous arena from which Slab instances are carved
// ---------------------------------------------------------------------------

struct alignas(CACHE_LINE_SIZE) Chunk {
    std::size_t capacity;
    std::size_t used;
    Chunk* next;

    std::byte _header_padding
        [(CACHE_LINE_SIZE
          - ((sizeof(std::size_t) * 2 + sizeof(Chunk*)) % CACHE_LINE_SIZE))
         % CACHE_LINE_SIZE];

    alignas(CACHE_LINE_SIZE) std::byte data[1];
};

static_assert(
    offsetof(Chunk, data) % CACHE_LINE_SIZE == 0,
    "Chunk::data must begin on a cache-line boundary");

static_assert(alignof(Chunk) == CACHE_LINE_SIZE, "Chunk must be cache-line aligned");

// ---------------------------------------------------------------------------
// Slab sizing helpers
// ---------------------------------------------------------------------------

[[nodiscard]] inline constexpr std::size_t slab_header_size() noexcept
{
    return offsetof(Slab, storage);
}

[[nodiscard]] inline constexpr std::size_t slab_stride_for_object(
    std::size_t object_size) noexcept
{
    const std::size_t minimum = object_size < sizeof(MemoryCell)
        ? sizeof(MemoryCell)
        : object_size;
    return align_up(minimum, CACHE_LINE_SIZE);
}

[[nodiscard]] inline constexpr std::size_t slab_total_size(
    std::size_t storage_bytes) noexcept
{
    return slab_header_size() + storage_bytes;
}

[[nodiscard]] inline std::size_t slab_cell_capacity(
    const Slab* slab) noexcept
{
    if (slab == nullptr || slab->object_size == 0) {
        return 0;
    }
    return slab->storage_bytes / slab->object_size;
}

// ---------------------------------------------------------------------------
// Slab initialization
// ---------------------------------------------------------------------------

void initialize_slab(Slab* slab, std::size_t object_size) noexcept;

} // namespace mae
