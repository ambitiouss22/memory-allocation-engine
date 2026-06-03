#include "thread_cache.hpp"

#include "central_pool.hpp"

namespace mae {

namespace {

thread_local ThreadCache g_thread_cache{};

} // namespace

ThreadCache& thread_cache() noexcept
{
    return g_thread_cache;
}

MemoryCell* fetch_batch_from_central(std::size_t class_index) noexcept
{
    if (class_index >= SIZE_CLASS_COUNT) {
        return nullptr;
    }

    MemoryCell* batch =
        pop_batch_from_central(class_index, CENTRAL_FALLBACK_BATCH);
    if (batch != nullptr) {
        return batch;
    }

    if (!replenish_central_pool(class_index)) {
        return nullptr;
    }

    return pop_batch_from_central(class_index, CENTRAL_FALLBACK_BATCH);
}

void* allocate_local(std::size_t size) noexcept
{
    const std::size_t class_index = size_to_class_index(size);
    if (class_index >= SIZE_CLASS_COUNT) {
        return nullptr;
    }

    ThreadCache& cache = thread_cache();
    MemoryCell*& head = cache.free_lists[class_index];

    if (head == nullptr) {
        head = fetch_batch_from_central(class_index);
        if (head == nullptr) {
            return nullptr;
        }
    }

    MemoryCell* cell = head;
    head = head->next;
    return static_cast<void*>(cell);
}

void deallocate_local(void* ptr, std::size_t size) noexcept
{
    if (ptr == nullptr) {
        return;
    }

    const std::size_t class_index = size_to_class_index(size);
    if (class_index >= SIZE_CLASS_COUNT) {
        return;
    }

    ThreadCache& cache = thread_cache();
    auto* cell = reinterpret_cast<MemoryCell*>(ptr);

    cell->next = cache.free_lists[class_index];
    cache.free_lists[class_index] = cell;
}

} // namespace mae
