#include "types.h"
#include "riscv.h"
#include "slab.h"
#include "buddy.h"
#include "defs.h"
#include "memlayout.h"

#define NULL ((void*)0)

#define MIN_BUFFER_SIZE (1<<5) // 2^5
#define MAX_BUFFER_SIZE (1<<17) // 2^17
#define NUM_BUFFER_CACHES 13 // 2^5 - 2^17

#define SLAB_PARTIAL 0
#define SLAB_FULL 1
#define SLAB_EMPTY 2

#define SLAB_ERR_DETECT 0xB16B00B5

#define OBJECTS_PER_SLAB(obj_size, slab_size) \
    (((slab_size) - sizeof(kmem_slab_t)) / ((obj_size) + sizeof(kmem_object_t)))


static kmem_cache_t *cache_list = NULL;
static struct spinlock cache_list_lock;
static int allocator_initialized = 0;

static kmem_cache_t *buffer_caches[NUM_BUFFER_CACHES];
static struct spinlock buffer_cache_lock;

static size_t round_up_pow2(size_t size) {
    size_t power = MIN_BUFFER_SIZE;
    while (power < size && power <= MAX_BUFFER_SIZE) {
        power <<= 1;
    }
    return power;
}

static int get_buffer_cache_index(size_t size) {
    int index = 0;
    size_t power = MIN_BUFFER_SIZE;
    while (power < size && index < NUM_BUFFER_CACHES - 1) {
        power <<= 1;
        index++;
    }
    return index;
}

static int calculate_slab_size(size_t object_size) {
    int blocks = 1;
    size_t slab_bytes = blocks * BLOCK_SIZE;

    while (blocks < 16) {
        size_t objects = OBJECTS_PER_SLAB(object_size, slab_bytes);
        if (objects >= 8) {
            break;
        }
        blocks <<= 1;
        slab_bytes = blocks * BLOCK_SIZE;
    }

    return blocks;
}

static void add_slab_to_list(kmem_slab_t **list_head, kmem_slab_t *slab) {
    slab->next = *list_head;
    slab->prev = NULL;

    if (*list_head) {
        (*list_head)->prev = slab;
    }
    *list_head = slab;
}

static void remove_slab_from_list(kmem_slab_t **list_head, kmem_slab_t *slab) {
    if (slab->prev) {
        slab->prev->next = slab->next;
    } else {
        *list_head = slab->next;
    }

    if (slab->next) {
        slab->next->prev = slab->prev;
    }

    slab->next = slab->prev = NULL;
}

static kmem_slab_t *kmem_slab_create(kmem_cache_t *cache) {
    void *mem = buddy_alloc(cache->slab_size);
    if (!mem) {
        return NULL;
    }

    kmem_slab_t *slab = (kmem_slab_t *)mem;
    slab->next = NULL;
    slab->prev = NULL;
    slab->mem = mem;

    size_t slab_bytes = cache->slab_size * BLOCK_SIZE;
    slab->num_objects_max = OBJECTS_PER_SLAB(cache->object_size, slab_bytes);
    slab->num_free = slab->num_objects_max;
    slab->cache = cache;

    // Object list starts after kmem_slab_t
    char *obj_start = (char *)mem + sizeof(kmem_slab_t);
    slab->free_list = NULL;

    for (int i = slab->num_objects_max - 1; i >= 0; i--) {
        char *obj_addr = obj_start + i * cache->aligned_size;
        kmem_object_t *object = (kmem_object_t *)obj_addr;
        object->next = (kmem_object_t *)slab->free_list;
        slab->free_list = object;
    }

    return slab;
}

static void kmem_slab_destroy(kmem_cache_t *cache, kmem_slab_t *slab) {
    if (!slab) return;

    if (cache->dtor) {
        char *obj_start = (char *)slab->mem + sizeof(kmem_slab_t);
        for (uint i = 0; i < slab->num_objects_max; i++) {
            char *obj_addr = obj_start + (i * cache->aligned_size);
            void *obj = obj_addr + sizeof(kmem_object_t);
            cache->dtor(obj);
        }
    }

    buddy_free(slab->mem, cache->slab_size);
}

// Grow cache by adding a new slab
static int kmem_cache_grow(kmem_cache_t *cache) {
    kmem_slab_t *slab = kmem_slab_create(cache);
    if (!slab) {
        cache->error_state = 1;
        return -1;
    }

    add_slab_to_list(&cache->slabs_empty, slab);
    cache->num_slabs++;
    cache->num_grown++;
    return 0;
}

void kmem_init(void *space, int block_num) {
    buddy_init(space, block_num);

    initlock(&cache_list_lock, "cache_list");
    initlock(&buffer_cache_lock, "buffer_cache");

    for (int i = 0; i < NUM_BUFFER_CACHES; i++) {
        buffer_caches[i] = NULL;
    }

    allocator_initialized = 1;
}

kmem_cache_t *kmem_cache_create(const char *name, size_t size,
                                void (*ctor)(void *),
                                void (*dtor)(void *)) {
    if (!allocator_initialized) {
        //panic("allocator not initialized");
        return NULL;
    }

    if (size == 0 || size > MAX_BUFFER_SIZE) {
        //panic("invalid size");
        return NULL;
    }

    kmem_cache_t *cache = (kmem_cache_t *)buddy_alloc(1);

    if (!cache) {
        return NULL;
    }

    memset(cache, 0, sizeof(kmem_cache_t));

    int name_len = strlen(name);
    if (name_len >= CACHE_NAME_LEN) {
        name_len = CACHE_NAME_LEN - 1;
    }
    memmove(cache->name, name, name_len);
    cache->name[name_len] = '\0';

    cache->object_size = size;
    cache->aligned_size = size + sizeof(kmem_object_t);
    cache->slab_size = calculate_slab_size(size);
    cache->error_detection = SLAB_ERR_DETECT;

    cache->ctor = ctor;
    cache->dtor = dtor;
    cache->error_state = 0;
    cache->last_shrink_state = 0;

    initlock(&cache->lock, cache->name);

    acquire(&cache_list_lock);
    cache->next = cache_list;
    cache_list = cache;
    release(&cache_list_lock);

    acquire(&cache->lock);
    kmem_cache_grow(cache);
    release(&cache->lock);

    return cache;
}

void *kmem_cache_alloc(kmem_cache_t *cachep) {
    if (!cachep || cachep->error_detection != SLAB_ERR_DETECT) {
        return NULL;
    }

    acquire(&cachep->lock);

    kmem_slab_t *slab = NULL;

    if (cachep->slabs_partial) {
        slab = cachep->slabs_partial;
    } else if (cachep->slabs_empty) {
        slab = cachep->slabs_empty;
    } else {
        if (kmem_cache_grow(cachep) < 0) {
            release(&cachep->lock);
            return NULL;
        }
        slab = cachep->slabs_empty;
    }

    if (!slab || !slab->free_list) {
        cachep->error_state = 1;
        release(&cachep->lock);
        return NULL;
    }

    kmem_object_t *object = (kmem_object_t *)slab->free_list;
    slab->free_list = object->next;
    object->slab = slab;
    slab->num_free--;
    cachep->num_allocated++;

    if (slab->num_free == slab->num_objects_max - 1) {
        remove_slab_from_list(&cachep->slabs_empty, slab);
        add_slab_to_list(&cachep->slabs_partial, slab);
    }

    if (slab->num_free == 0) {
        remove_slab_from_list(&cachep->slabs_partial, slab);
        add_slab_to_list(&cachep->slabs_full, slab);
    }

    if (cachep->ctor) {
        //cachep->ctor((void *)((char *)object + sizeof(kmem_object_t)));
    }

    release(&cachep->lock);

    return (void *)((char *)object + sizeof(kmem_object_t));
}

void kmem_cache_free(kmem_cache_t *cachep, void *objp) {
    if (!cachep || !objp || cachep->error_detection != SLAB_ERR_DETECT) {
        return;
    }

    kmem_object_t *object = (kmem_object_t *)((char *)objp - sizeof(kmem_object_t));

    acquire(&cachep->lock);

    kmem_slab_t *slab = object->slab;

    if (!slab) {
        cachep->error_state = 1;
        release(&cachep->lock);
        return;
    }

    object->next = (kmem_object_t *)slab->free_list;
    slab->free_list = object;
    slab->num_free++;
    cachep->num_allocated--;

    if (slab->num_free == 1) {
        remove_slab_from_list(&cachep->slabs_full, slab);
        add_slab_to_list(&cachep->slabs_partial, slab);
    }
    if (slab->num_free == slab->num_objects_max) {
        remove_slab_from_list(&cachep->slabs_partial, slab);
        add_slab_to_list(&cachep->slabs_empty, slab);
    }

    release(&cachep->lock);
}

int kmem_cache_shrink(kmem_cache_t *cachep) {
    if (!cachep || cachep->error_detection != SLAB_ERR_DETECT) {
        return -1;
    }

    acquire(&cachep->lock);

    if (cachep->num_grown > cachep->last_shrink_state) {
        // Cache has grown since last shrink, so don't shrink
        cachep->last_shrink_state = cachep->num_grown;
        release(&cachep->lock);
        return 0;
    }

    int blocks_freed = 0;

    kmem_slab_t *slab = cachep->slabs_empty;
    while (slab) {
        kmem_slab_t *next = slab->next;
        blocks_freed += cachep->slab_size;
        remove_slab_from_list(&cachep->slabs_empty, slab);
        kmem_slab_destroy(cachep, slab);
        cachep->num_slabs--;
        slab = next;
    }

    if (blocks_freed > 0) {
        cachep->num_shrunk++;
    }

    release(&cachep->lock);

    return blocks_freed;
}

void kmem_cache_destroy(kmem_cache_t *cachep) {
    if (!cachep || cachep->error_detection != SLAB_ERR_DETECT) {
        return;
    }

    acquire(&cachep->lock);

    kmem_slab_t *slab;

    while (cachep->slabs_empty) {
        slab = cachep->slabs_empty;
        remove_slab_from_list(&cachep->slabs_empty, slab);
        kmem_slab_destroy(cachep, slab);
    }

    while (cachep->slabs_partial) {
        slab = cachep->slabs_partial;
        remove_slab_from_list(&cachep->slabs_partial, slab);
        kmem_slab_destroy(cachep, slab);
    }

    while (cachep->slabs_full) {
        slab = cachep->slabs_full;
        remove_slab_from_list(&cachep->slabs_full, slab);
        kmem_slab_destroy(cachep, slab);
    }

    acquire(&cache_list_lock);
    kmem_cache_t **pp = &cache_list;
    while (*pp) {
        if (*pp == cachep) {
            *pp = cachep->next;
            break;
        }
        pp = &(*pp)->next;
    }
    release(&cache_list_lock);

    cachep->error_detection = 0; // Invalidate cache.
    release(&cachep->lock);

    buddy_free(cachep, 1);
}

void *kmalloc(size_t size) {
    if (!allocator_initialized || size == 0 || size > MAX_BUFFER_SIZE) {
        return NULL;
    }

    size_t rounded_size = round_up_pow2(size);
    int index = get_buffer_cache_index(rounded_size);

    acquire(&buffer_cache_lock);

    if (!buffer_caches[index]) {
        // Set the buffer name to its size
        char name[32];
        char *p = name;
        *p++ = 's'; *p++ = 'i'; *p++ = 'z'; *p++ = 'e'; *p++ = '-';

        // Convert size to string
        size_t temp = rounded_size;
        char digits[20];
        int digit_count = 0;
        do {
            digits[digit_count++] = '0' + (temp % 10);
            temp /= 10;
        } while (temp > 0);

        for (int i = digit_count - 1; i >= 0; i--) {
            *p++ = digits[i];
        }
        *p = '\0';

        release(&buffer_cache_lock);

        kmem_cache_t *cache = kmem_cache_create(name, rounded_size, NULL, NULL);

        acquire(&buffer_cache_lock);
        buffer_caches[index] = cache;
    }

    kmem_cache_t *cache = buffer_caches[index];
    release(&buffer_cache_lock);

    if (!cache) {
        return NULL;
    }
    
    return kmem_cache_alloc(cache);
}

void kfree(const void *objp) {
    if (!objp) {
        return;
    }

    acquire(&buffer_cache_lock);

    kmem_object_t *object = (kmem_object_t *)((char *)objp - sizeof(kmem_object_t));
    kmem_slab_t *slab = object->slab;
    kmem_cache_t *cache = slab->cache;

    kmem_cache_free(cache, (void *)objp);

    release(&buffer_cache_lock);
}

void kmem_cache_info(kmem_cache_t *cachep) {
    if (!cachep || cachep->error_detection != SLAB_ERR_DETECT) {
        printf("Invalid cache\n");
        return;
    }
    
    acquire(&cachep->lock);
    uint objects_per_slab = OBJECTS_PER_SLAB(cachep->object_size, cachep->slab_size * BLOCK_SIZE);
    uint total_objects = cachep->num_slabs * objects_per_slab;
    uint percent_used = 0;
    if (total_objects > 0) {
        percent_used = (cachep->num_allocated * 100) / total_objects;
    }
    
    int total_blocks = cachep->num_slabs * cachep->slab_size;
    
    printf("\nCache: %s\n", cachep->name);
    printf("\tObject size: %d bytes\n", (int)cachep->object_size);
    printf("\tCache size: %d blocks\n", total_blocks);
    printf("\tNumber of slabs: %d\n", cachep->num_slabs);
    printf("\tObjects per slab: %d\n", objects_per_slab);
    printf("\tUtilization: %d%%\n", percent_used);
    printf("\tAllocated objects: %d / %d\n", cachep->num_allocated, total_objects);
    
    release(&cachep->lock);
}

int kmem_cache_error(kmem_cache_t *cachep) {
    if (!cachep || cachep->error_detection != SLAB_ERR_DETECT) {
        return 1;
    }
    
    acquire(&cachep->lock);
    int error = cachep->error_state;
    release(&cachep->lock);
    
    return error;
}