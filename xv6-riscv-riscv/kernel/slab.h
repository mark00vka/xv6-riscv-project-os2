#ifndef _SLAB_H_
#define _SLAB_H_

#define BLOCK_SIZE (4096)

typedef unsigned long size_t;

typedef struct kmem_cache_s kmem_cache_t;

/**
 * Initialize the slab allocator system
 * @param space Pointer to the start of memory space to manage
 * @param block_num Number of blocks in the memory space
 */
void kmem_init(void *space, int block_num);

/**
 * Create a new cache for objects of a specific size
 * @param name Name of the cache (for debugging/info)
 * @param size Size of each object in bytes
 * @param ctor Constructor function called when objects are initialized (can be NULL)
 * @param dtor Destructor function called when objects are destroyed (can be NULL)
 * @return Pointer to the cache handle, or NULL on failure
 */
kmem_cache_t *kmem_cache_create(const char *name, size_t size,
                                void (*ctor)(void *),
                                void (*dtor)(void *));

/**
 * Shrink a cache by freeing empty slabs
 * @param cachep Pointer to the cache
 * @return Number of blocks freed, or -1 on error
 */
int kmem_cache_shrink(kmem_cache_t *cachep);

/**
 * Allocate one object from the cache
 * @param cachep Pointer to the cache
 * @return Pointer to the allocated object, or NULL on failure
 */
void *kmem_cache_alloc(kmem_cache_t *cachep);

/**
 * Free one object back to the cache
 * @param cachep Pointer to the cache
 * @param objp Pointer to the object to free
 */
void kmem_cache_free(kmem_cache_t *cachep, void *objp);

/**
 * Allocate a small memory buffer of arbitrary size
 * Size is rounded up to nearest power of 2 between 2^5 and 2^17
 * @param size Size in bytes
 * @return Pointer to allocated memory, or NULL on failure
 */
void *kmalloc(size_t size);

/**
 * Free a small memory buffer allocated by kmalloc
 * @param objp Pointer to the memory to free
 */
void kfree(const void *objp);

/**
 * Destroy a cache and free all its resources
 * @param cachep Pointer to the cache
 */
void kmem_cache_destroy(kmem_cache_t *cachep);

/**
 * Print information about a cache
 * @param cachep Pointer to the cache
 */
void kmem_cache_info(kmem_cache_t *cachep);

/**
 * Check for errors in cache state
 * @param cachep Pointer to the cache
 * @return 0 if no error, non-zero if error detected
 */
int kmem_cache_error(kmem_cache_t *cachep);

#endif // _SLAB_H_