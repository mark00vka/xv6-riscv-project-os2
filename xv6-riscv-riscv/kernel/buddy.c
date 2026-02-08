#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#include "buddy.h"

#define MAX_ORDER 10

struct buddy_block {
    struct buddy_block *next;
};

struct {
    struct spinlock lock;
    struct buddy_block *free_list[MAX_ORDER + 1];
    char *memory_start;
    int total_blocks;
    int free_blocks;
} buddy;

void buddy_init(void *space, int block_num) {
    initlock(&buddy.lock, "buddy");
    buddy.memory_start = (char*)space;
    buddy.total_blocks = block_num;
    buddy.free_blocks = block_num;

    for (int i = 0; i <= MAX_ORDER; i++) {
        buddy.free_list[i] = 0;
    }

    // Add all blocks to largest possible chunks
    int remaining = block_num;
    char *current = (char*)space;

    while (remaining > 0) {
        int order = MAX_ORDER;
        while ((1 << order) > remaining) {
            order--;
        }

        struct buddy_block *block = (struct buddy_block*)current;
        block->next = buddy.free_list[order];
        buddy.free_list[order] = block;

        int size = (1 << order);
        current += size * BLOCK_SIZE;
        remaining -= size;
    }
}

void *buddy_alloc(int num_blocks) {
    acquire(&buddy.lock);

    // Find order needed
    int order = 0;
    while ((1 << order) < num_blocks) {
        order++;
    }

    if (order > MAX_ORDER) {
        release(&buddy.lock);
        return 0;
    }

    // Find available block
    int found_order = order;
    while (found_order <= MAX_ORDER && buddy.free_list[found_order] == 0) {
        found_order++;
    }

    if (found_order > MAX_ORDER) {
        release(&buddy.lock);
        return 0;
    }

    // Get block from free list
    struct buddy_block *block = buddy.free_list[found_order];
    buddy.free_list[found_order] = block->next;

    // Split if necessary
    while (found_order > order) {
        found_order--;
        int buddy_size = (1 << found_order) * BLOCK_SIZE;
        struct buddy_block *buddy_block = (struct buddy_block*)((char*)block + buddy_size);
        buddy_block->next = buddy.free_list[found_order];
        buddy.free_list[found_order] = buddy_block;
    }

    buddy.free_blocks -= (1 << order);
    release(&buddy.lock);
    return (void*)block;
}

void buddy_free(void *ptr, int num_blocks) {
    acquire(&buddy.lock);

    int order = 0;
    while ((1 << order) < num_blocks) {
        order++;
    }

    // Add back to free list (simplified - no coalescing)
    struct buddy_block *block = (struct buddy_block*)ptr;
    block->next = buddy.free_list[order];
    buddy.free_list[order] = block;

    buddy.free_blocks += (1 << order);
    release(&buddy.lock);
}

int buddy_get_free_blocks(void) {
    return buddy.free_blocks;
}