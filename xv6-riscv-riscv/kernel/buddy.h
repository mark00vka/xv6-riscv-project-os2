// File: buddy.h
// Buddy allocator interface - manages physical memory blocks

#ifndef _BUDDY_H_
#define _BUDDY_H_

#define BLOCK_SIZE (4096)

/**
 * Initialize the buddy allocator with a memory region
 * @param space Pointer to the start of memory space
 * @param block_num Number of blocks in the memory space
 */
void buddy_init(void *space, int block_num);

/**
 * Allocate a contiguous region of blocks
 * @param num_blocks Number of blocks to allocate (must be power of 2)
 * @return Pointer to the allocated memory, or NULL if allocation fails
 */
void *buddy_alloc(int num_blocks);

/**
 * Free a previously allocated region
 * @param ptr Pointer to the memory to free
 * @param num_blocks Number of blocks being freed (must match allocation size)
 */
void buddy_free(void *ptr, int num_blocks);

/**
 * Get the total number of free blocks available
 * @return Number of free blocks
 */
int buddy_get_free_blocks(void);

#endif // _BUDDY_H_