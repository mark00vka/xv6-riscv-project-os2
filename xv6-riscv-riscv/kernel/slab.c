#include "slab.h"

void kmem_init(void *space, int block_num) {
    // Initialize slab allocator
}

kmem_cache_t *kmem_cache_create(const char *name, size_t size,
                                void (*ctor)(void *),
                                void (*dtor)(void *)) {
    return 0;
}

int kmem_cache_shrink(kmem_cache_t *cachep) {
    return 0;
}

void *kmem_cache_alloc(kmem_cache_t *cachep) {
    return 0;
}

void kmem_cache_free(kmem_cache_t *cachep, void *objp) {
}

void *kmalloc(size_t size) {
    return 0;
}

// void kfree(void *objp) {
// }

void kmem_cache_destroy(kmem_cache_t *cachep) {
}

void kmem_cache_info(kmem_cache_t *cachep) {
}

int kmem_cache_error(kmem_cache_t *cachep) {
    return 0;
}
