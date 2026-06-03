#include "central_pool.hpp"

#include "slab_allocator.hpp"

#include <cstdlib>
#include <new>

namespace mae {

namespace {

[[nodiscard]] TaggedHead load_head(
    const std::atomic<TaggedHead>& head) noexcept
{
    return head.load(std::memory_order_acquire);
}

[[nodiscard]] bool cas_head(
    std::atomic<TaggedHead>& head,
    TaggedHead& expected,
    TaggedHead desired) noexcept
{
    return head.compare_exchange_weak(
        expected,
        desired,
        std::memory_order_acq_rel,
        std::memory_order_acquire);
}

// Single-threaded: chain is privately owned — no concurrent access.
[[nodiscard]] MemoryCell* split_batch(
    MemoryCell* chain,
    std::size_t batch_size,
    MemoryCell*& remainder) noexcept
{
    if (chain == nullptr) {
        remainder = nullptr;
        return nullptr;
    }

    MemoryCell* batch_tail = chain;
    std::size_t count = 1;

    while (count < batch_size && batch_tail->next != nullptr) {
        batch_tail = batch_tail->next;
        ++count;
    }

    remainder = batch_tail->next;
    batch_tail->next = nullptr;
    return chain;
}

[[nodiscard]] MemoryCell* list_tail(MemoryCell* head) noexcept
{
    if (head == nullptr) {
        return nullptr;
    }

    MemoryCell* tail = head;
    while (tail->next != nullptr) {
        tail = tail->next;
    }

    return tail;
}

} // namespace

CentralPool& central_pool() noexcept
{
    static CentralPool instance{};
    return instance;
}

MemoryCell* pop_batch_from_central(
    std::size_t class_index,
    std::size_t batch_size) noexcept
{
    if (class_index >= SIZE_CLASS_COUNT || batch_size == 0) {
        return nullptr;
    }

    std::atomic<TaggedHead>& head = central_pool().lists[class_index].head;

    while (true) {
        const TaggedHead snapshot = load_head(head);
        if (snapshot.ptr == nullptr) {
            return nullptr;
        }

        TaggedHead expected = snapshot;
        const TaggedHead desired{nullptr, snapshot.tag + 1};

        if (!cas_head(head, expected, desired)) {
            continue;
        }

        // Exclusive ownership of snapshot.ptr chain begins here.
        MemoryCell* remainder = nullptr;
        MemoryCell* const batch =
            split_batch(snapshot.ptr, batch_size, remainder);

        if (remainder != nullptr) {
            MemoryCell* const rem_tail = list_tail(remainder);
            push_batch_to_central(remainder, rem_tail, class_index);
        }

        return batch;
    }
}

void push_batch_to_central(
    MemoryCell* batch_head,
    MemoryCell* batch_tail,
    std::size_t class_index) noexcept
{
    if (batch_head == nullptr || batch_tail == nullptr || class_index >= SIZE_CLASS_COUNT) {
        return;
    }

    std::atomic<TaggedHead>& head = central_pool().lists[class_index].head;

    while (true) {
        const TaggedHead snapshot = load_head(head);
        batch_tail->next = snapshot.ptr;

        TaggedHead expected = snapshot;
        const TaggedHead desired{batch_head, snapshot.tag + 1};

        if (cas_head(head, expected, desired)) {
            return;
        }
    }
}

bool replenish_central_pool(std::size_t class_index) noexcept
{
    if (class_index >= SIZE_CLASS_COUNT) {
        return false;
    }

    const std::size_t allocation_size =
        align_up(chunk_total_size(DEFAULT_CHUNK_DATA_BYTES), CACHE_LINE_SIZE);

    void* const raw = ::operator new(allocation_size, std::align_val_t{CACHE_LINE_SIZE});
    if (raw == nullptr) {
        return false;
    }

    auto* chunk = new (raw) Chunk{};
    chunk->capacity = DEFAULT_CHUNK_DATA_BYTES;
    chunk->used = DEFAULT_CHUNK_DATA_BYTES;
    chunk->next = nullptr;

    const std::size_t storage_bytes = DEFAULT_CHUNK_DATA_BYTES - slab_header_size();
    if (storage_bytes == 0) {
        return false;
    }

    auto* slab = new (chunk->data) Slab{};
    slab->storage_bytes = storage_bytes;

    initialize_slab(slab, size_class_allocation_bytes(class_index));

    MemoryCell* const batch_head = slab->free_list_head;
    if (batch_head == nullptr) {
        return false;
    }

    MemoryCell* batch_tail = batch_head;
    while (batch_tail->next != nullptr) {
        batch_tail = batch_tail->next;
    }

    slab->free_list_head = nullptr;
    push_batch_to_central(batch_head, batch_tail, class_index);
    return true;
}

} // namespace mae
