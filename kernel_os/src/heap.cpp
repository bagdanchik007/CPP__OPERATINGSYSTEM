#include "heap.h"
#include <stdint.h>

namespace {
    constexpr size_t HEAP_SIZE = 64 * 1024;
    constexpr size_t HEAP_ALIGNMENT = alignof(uintptr_t);

    struct BlockHeader {
        size_t size;
        bool free;
        BlockHeader* next;
    };

    alignas(uintptr_t) uint8_t heap_area[HEAP_SIZE];
    BlockHeader* first_block = nullptr;
    bool heap_initialized = false;

    constexpr size_t align_up(size_t value) {
        return (value + HEAP_ALIGNMENT - 1) & ~(HEAP_ALIGNMENT - 1);
    }

    bool blocks_are_adjacent(const BlockHeader* first, const BlockHeader* second) {
        const uint8_t* first_end = reinterpret_cast<const uint8_t*>(first) +
                                   sizeof(BlockHeader) + first->size;
        return first_end == reinterpret_cast<const uint8_t*>(second);
    }

    void split_block(BlockHeader* block, size_t size) {
        if (block->size < size + sizeof(BlockHeader) + HEAP_ALIGNMENT) {
            return;
        }

        uint8_t* new_block_address = reinterpret_cast<uint8_t*>(block) +
                                     sizeof(BlockHeader) + size;
        BlockHeader* new_block = reinterpret_cast<BlockHeader*>(new_block_address);
        new_block->size = block->size - size - sizeof(BlockHeader);
        new_block->free = true;
        new_block->next = block->next;

        block->size = size;
        block->next = new_block;
    }

    void coalesce_free_blocks() {
        for (BlockHeader* block = first_block; block && block->next; ) {
            BlockHeader* next = block->next;
            if (block->free && next->free && blocks_are_adjacent(block, next)) {
                block->size += sizeof(BlockHeader) + next->size;
                block->next = next->next;
            } else {
                block = next;
            }
        }
    }
}

extern "C" void heap_init() {
    first_block = reinterpret_cast<BlockHeader*>(heap_area);
    first_block->size = HEAP_SIZE - sizeof(BlockHeader);
    first_block->free = true;
    first_block->next = nullptr;
    heap_initialized = true;
}

extern "C" void* kmalloc(size_t size) {
    if (size == 0 || size > HEAP_SIZE - sizeof(BlockHeader)) {
        return nullptr;
    }

    if (!heap_initialized) {
        heap_init();
    }

    size = align_up(size);
    for (BlockHeader* block = first_block; block; block = block->next) {
        if (block->free && block->size >= size) {
            split_block(block, size);
            block->free = false;
            return reinterpret_cast<uint8_t*>(block) + sizeof(BlockHeader);
        }
    }

    return nullptr;
}

extern "C" void kfree(void* pointer) {
    if (!pointer) {
        return;
    }

    uint8_t* address = reinterpret_cast<uint8_t*>(pointer);
    for (BlockHeader* block = first_block; block; block = block->next) {
        uint8_t* block_data = reinterpret_cast<uint8_t*>(block) + sizeof(BlockHeader);
        if (block_data == address) {
            block->free = true;
            coalesce_free_blocks();
            return;
        }
    }
}
