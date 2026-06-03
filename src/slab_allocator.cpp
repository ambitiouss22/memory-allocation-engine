#include "slab_allocator.hpp"

namespace mae {

void initialize_slab(Slab* slab, std::size_t object_size) noexcept
{
    if (slab == nullptr) {
        return;
    }

    const std::size_t stride = slab_stride_for_object(object_size);
    slab->object_size = stride;
    slab->free_list_head = nullptr;

    if (slab->storage_bytes == 0) {
        slab->cell_count = 0;
        return;
    }

    const std::size_t cell_count = slab->storage_bytes / stride;
    slab->cell_count = cell_count;

    std::byte* cursor = slab->storage;
    const std::byte* const end = slab->storage + (cell_count * stride);

    while (cursor < end) {
        auto* cell = reinterpret_cast<MemoryCell*>(cursor);
        cell->next = slab->free_list_head;
        slab->free_list_head = cell;
        cursor += stride;
    }
}

} // namespace mae
