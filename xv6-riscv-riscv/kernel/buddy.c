#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#include "buddy.h"

#define MAX_ORDER 15

struct buddy_block {
    struct buddy_block *next;
};

struct {
    int max_order;
    struct spinlock lock;
    struct buddy_block *free_list[MAX_ORDER];
    char *memory_start;
    int total_blocks;
    int free_blocks;
} buddy;

int log2_floor(int x) {
    int result = 0;
    while (x > 1) {
        x >>= 1;
        result++;
    }
    return result;
}

int log2_ceil(int x) {
    if (x <= 1) return 0;
    int result = 0;
    int power = 1;
    while (power < x) {
        power <<= 1;
        result++;
    }
    return result;
}

int round_up_pow2(size_t size) {
    size_t power = 1;
    while (power < size) power <<= 1;
    return power;
}

/* 0 1 [2 3] 4 5 6 7                     10 xor 10 = 0
 *      ^ block_index = 2; buddy_index = 2 xor 2^1 = 0
 * 0 1 2 3 4 5 [6] 7                           110 xor 001 = 111
 *              ^ block_index = 6; buddy_index = 6 xor 2^0 = 7 */
void *get_buddy(void *ptr, int order) {
    uint64 block_index = ((char *)ptr - buddy.memory_start) / BLOCK_SIZE;
    uint64 buddy_index = block_index ^ (1 << order);
    return buddy.memory_start + buddy_index * BLOCK_SIZE;
}

/* 0 1 [2 3] 4 5 6 7                           10 & 01 = 0
 *      ^ block_index = 2; parent_index = 2 & not(2^1) = 0
 * 0 1 2 3 4 5 [6] 7                              110 & 110 = 6
 *              ^ block_index = 6; parent_index = 6 & not(2^0) = 6 */
void *get_parent(void *ptr, int order) {
    uint64 block_index = ((char *)ptr - buddy.memory_start) / BLOCK_SIZE;
    uint64 parent_index = block_index & ~(1 << order);
    return buddy.memory_start + parent_index * BLOCK_SIZE;
}

void remove_from_list(struct buddy_block *block, int order) {
    struct buddy_block *head = buddy.free_list[order];
    if (head == block) {
        buddy.free_list[order] = block->next;
        return;
    }

    struct buddy_block *current = head;
    while (current && current->next != block) {
        current = current->next;
    }

    if (current) {
        current->next = block->next;
    }
}

void add_to_list(void *ptr, int order) {
    struct buddy_block *block = (struct buddy_block *)ptr;
    block->next = buddy.free_list[order];
    buddy.free_list[order] = block;
}

uint is_block_free(struct buddy_block *block, int order) {
    if (order < 0 || order >= buddy.max_order) return 0;
    struct buddy_block *next = buddy.free_list[order];
    while (next) {
        if (block == next) return 1;
        next = next->next;
    }
    return 0;
}

void buddy_init(void *space, int block_num) {
    if (block_num < 1) return;

    int max_order = log2_ceil(block_num)+1;
    if (max_order > MAX_ORDER) return;

    initlock(&buddy.lock, "buddy");
    buddy.total_blocks = block_num;
    buddy.free_blocks = block_num;
    buddy.max_order = max_order;

    int free_list_size = max_order * sizeof(struct buddy_block *);
    memset(buddy.free_list, 0, free_list_size);

    buddy.memory_start = (char*)space;
    struct buddy_block *current = (struct buddy_block*)buddy.memory_start;
    int remaining = block_num;
    int order = max_order;

    // 23 -> [16][4][2][1] | log2(23) ~ 4 is max_order
    while (remaining > 0) {
        while (1 << order > remaining) {
            order--;
        }

        add_to_list(current, order);

        int size = 1 << order;
        current += size * BLOCK_SIZE;
        remaining -= size;
    }
}

void *buddy_alloc(int num_blocks) {
    if (num_blocks < 1) return 0;

    int order = log2_ceil(num_blocks);

    acquire(&buddy.lock);

    if (order > buddy.max_order) {
        release(&buddy.lock);
        return 0;
    }

    int found_order = order;
    while (found_order <= buddy.max_order && buddy.free_list[found_order] == 0) {
        found_order++;
    }

    if (found_order > buddy.max_order) {
        release(&buddy.lock);
        return 0;
    }

    struct buddy_block *block = buddy.free_list[found_order];
    remove_from_list(block, found_order);

    while (found_order > order) {
        found_order--;
        int buddy_size = (1 << found_order) * BLOCK_SIZE;
        struct buddy_block *buddy_block = (struct buddy_block*)((char*)block + buddy_size);
        buddy_block->next = buddy.free_list[found_order];
        buddy.free_list[found_order] = buddy_block;
    }

    buddy.free_blocks -= 1 << order;
    release(&buddy.lock);
    return (void*)block;
}

void buddy_free(void *ptr, int num_blocks) {
    if (!ptr || num_blocks < 1) return;

    acquire(&buddy.lock);

    int order = log2_ceil(num_blocks);

    while (order < buddy.max_order) {
        void *buddy_ptr = get_buddy(ptr, order);

        if (is_block_free(buddy_ptr, order)) {
            remove_from_list(buddy_ptr, order);
            ptr = get_parent(ptr, order);
            order++;
        } else break;
    }

    add_to_list(ptr, order);

    buddy.free_blocks += 1 << log2_ceil(num_blocks);
    release(&buddy.lock);
}

void buddy_print_info(void) {
    acquire(&buddy.lock);

    printf("\n=== Buddy Allocator Info ===\n");
    printf("Total blocks: %d (%d KB)\n",
           buddy.total_blocks,
           (buddy.total_blocks * BLOCK_SIZE) / 1024);
    printf("Free blocks: %d (%d KB)\n",
           buddy.free_blocks,
           (buddy.free_blocks * BLOCK_SIZE) / 1024);
    printf("Used blocks: %d (%d KB)\n",
           buddy.total_blocks - buddy.free_blocks,
           ((buddy.total_blocks - buddy.free_blocks) * BLOCK_SIZE) / 1024);
    printf("Utilization: %d%%\n",
           ((buddy.total_blocks - buddy.free_blocks) * 100) / buddy.total_blocks);

    printf("\nFree lists by order:\n");
    for (int order = buddy.max_order - 1; order >= 0; order--) {
        int count = 0;
        struct buddy_block *block = buddy.free_list[order];
        while (block) {
            count++;
            block = block->next;
        }

        if (count > 0) {
            int size = 1 << order;
            printf("  Order %d: %d blocks (%d KB) - %d chunks\n",
                   order, size, (size * BLOCK_SIZE) / 1024, count);
        }
    }
    printf("============================\n\n");

    release(&buddy.lock);
}

void pretty_print_buddy() {
    acquire(&buddy.lock);

    char *current = buddy.memory_start;
    char *end = buddy.memory_start + buddy.total_blocks * BLOCK_SIZE;
    int line_len = 0;
    const int max_line = 128;

    while (current < end) {
        int found_free = 0;
        int found_order = -1;

        for (int order = buddy.max_order; order >= 0; order--) {
            if (is_block_free((struct buddy_block *)current, order)) {
                found_free = 1;
                found_order = order;
                break;
            }
        }

        if (found_free) {
            int num_blocks = 1 << found_order;
            
            // Calculate label length manually since we don't have snprintf/strlen
            int label_len = 0;
            int temp = num_blocks;
            if (temp == 0) label_len = 1;
            while (temp > 0) {
                temp /= 10;
                label_len++;
            }

            int inner_width = num_blocks < max_line ? num_blocks : max_line;
            if (inner_width < label_len) inner_width = label_len;
            
            int block_width = inner_width + 2; // +2 for brackets
            
            if (line_len + block_width > max_line && line_len > 0) {
                printf("\n");
                line_len = 0;
            }

            int pad_left = (inner_width - label_len) / 2;
            int pad_right = inner_width - label_len - pad_left;

            printf("[");
            for (int i = 0; i < pad_left; i++) printf("-");
            printf("%d", num_blocks);
            for (int i = 0; i < pad_right; i++) printf("-");
            printf("]");
            
            current += num_blocks * BLOCK_SIZE;
            line_len += block_width;
        } else {
            if (line_len + 2 > max_line && line_len > 0) {
                printf("\n");
                line_len = 0;
            }
            printf("..");
            current += BLOCK_SIZE;
            line_len += 2;
        }
    }
    printf("\n");

    release(&buddy.lock);
}