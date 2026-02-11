#include "../kernel/types.h"
#include "../kernel/stat.h"
#include "user.h"

// Test structure
struct test_obj {
    int id;
    char data[56];
};

void
test_basic_cache(void)
{
    printf("TEST 1: Basic Cache Operations\n");
    printf("================================\n");

    kmem_cache_t *cache = kmem_cache_create("test_cache", sizeof(struct test_obj), 0, 0);
    if(cache == 0) {
        printf("FAIL: Could not create cache\n");
        return;
    }
    printf("PASS: Cache created\n");

    // Print initial info
    kmem_cache_info(cache);

    // Allocate objects
    struct test_obj *obj1 = (struct test_obj *)kmem_cache_alloc(cache);
    struct test_obj *obj2 = (struct test_obj *)kmem_cache_alloc(cache);
    struct test_obj *obj3 = (struct test_obj *)kmem_cache_alloc(cache);

    if(obj1 && obj2 && obj3) {
        printf("PASS: Allocated 3 objects\n");

        obj1->id = 1;
        obj2->id = 2;
        obj3->id = 3;

        printf("Object IDs: %d, %d, %d\n", obj1->id, obj2->id, obj3->id);
    } else {
        printf("FAIL: Could not allocate objects\n");
    }

    // Print cache info after allocation
    kmem_cache_info(cache);

    // Free objects
    kmem_cache_free(cache, obj1);
    kmem_cache_free(cache, obj2);
    kmem_cache_free(cache, obj3);
    printf("PASS: Freed 3 objects\n");

    // Check for errors
    int error = kmem_cache_error(cache);
    if(error) {
        printf("FAIL: Cache has errors\n");
    } else {
        printf("PASS: No cache errors\n");
    }

    // Destroy cache
    kmem_cache_destroy(cache);
    printf("PASS: Cache destroyed\n\n");
}

void
test_cache_growth(void)
{
    printf("TEST 2: Cache Growth\n");
    printf("====================\n");

    kmem_cache_t *cache = kmem_cache_create("growth_test", 64, 0, 0);
    if(cache == 0) {
        printf("FAIL: Could not create cache\n");
        return;
    }

    // Allocate many objects
    #define NUM_OBJS 50
    void *objs[NUM_OBJS];
    int i;

    for(i = 0; i < NUM_OBJS; i++) {
        objs[i] = kmem_cache_alloc(cache);
        if(objs[i] == 0) {
            printf("FAIL: Allocation failed at object %d\n", i);
            break;
        }
    }

    if(i == NUM_OBJS) {
        printf("PASS: Allocated %d objects\n", NUM_OBJS);
    }

    kmem_cache_info(cache);

    // Free all
    for(i = 0; i < NUM_OBJS; i++) {
        if(objs[i]) {
            kmem_cache_free(cache, objs[i]);
        }
    }
    printf("PASS: Freed all objects\n");

    // Try to shrink
    int freed = kmem_cache_shrink(cache);
    printf("Shrink freed %d blocks\n", freed);

    kmem_cache_info(cache);
    kmem_cache_destroy(cache);
    printf("PASS: Test completed\n\n");
}

void
test_slab_alloc(void)
{
    printf("TEST 3: slab_alloc/slab_free\n");
    printf("=============================\n");

    // Allocate buffers of various sizes
    void *buf1 = kmalloc(50);
    void *buf2 = kmalloc(100);
    void *buf3 = kmalloc(1000);

    if(buf1 && buf2 && buf3) {
        printf("PASS: Allocated 3 buffers\n");

        // Use the buffers
        char *cb1 = (char *)buf1;
        char *cb2 = (char *)buf2;
        char *cb3 = (char *)buf3;

        cb1[0] = 'A';
        cb2[0] = 'B';
        cb3[0] = 'C';

        if(cb1[0] == 'A' && cb2[0] == 'B' && cb3[0] == 'C') {
            printf("PASS: Buffers are usable\n");
        } else {
            printf("FAIL: Buffer corruption\n");
        }

        // Free buffers
        kfree(buf1);
        kfree(buf2);
        kfree(buf3);
        printf("PASS: Freed all buffers\n");
    } else {
        printf("FAIL: Could not allocate buffers\n");
    }

    printf("\n");
}

void
test_multiple_caches(void)
{
    printf("TEST 4: Multiple Caches\n");
    printf("========================\n");

    kmem_cache_t *cache1 = kmem_cache_create("cache1", 32, 0, 0);
    kmem_cache_t *cache2 = kmem_cache_create("cache2", 64, 0, 0);
    kmem_cache_t *cache3 = kmem_cache_create("cache3", 128, 0, 0);

    if(cache1 && cache2 && cache3) {
        printf("PASS: Created 3 caches\n");

        void *obj1 = kmem_cache_alloc(cache1);
        void *obj2 = kmem_cache_alloc(cache2);
        void *obj3 = kmem_cache_alloc(cache3);

        if(obj1 && obj2 && obj3) {
            printf("PASS: Allocated from each cache\n");

            kmem_cache_free(cache1, obj1);
            kmem_cache_free(cache2, obj2);
            kmem_cache_free(cache3, obj3);
            printf("PASS: Freed all objects\n");
        } else {
            printf("FAIL: Could not allocate from all caches\n");
        }

        kmem_cache_destroy(cache1);
        kmem_cache_destroy(cache2);
        kmem_cache_destroy(cache3);
        printf("PASS: All caches destroyed\n");
    } else {
        printf("FAIL: Could not create all caches\n");
    }

    printf("\n");
}

void
test_stress(void)
{
    printf("TEST 5: Stress Test\n");
    printf("====================\n");

    kmem_cache_t *cache = kmem_cache_create("stress", 128, 0, 0);
    if(cache == 0) {
        printf("FAIL: Could not create cache\n");
        return;
    }

    #define STRESS_OBJS 30
    void *objs[STRESS_OBJS];
    int i;

    // Pattern: allocate all, free half, allocate quarter, free all

    // Allocate all
    for(i = 0; i < STRESS_OBJS; i++) {
        objs[i] = kmem_cache_alloc(cache);
    }
    printf("Allocated %d objects\n", STRESS_OBJS);

    // Free half
    for(i = 0; i < STRESS_OBJS / 2; i++) {
        kmem_cache_free(cache, objs[i]);
        objs[i] = 0;
    }
    printf("Freed %d objects\n", STRESS_OBJS / 2);

    // Allocate quarter
    for(i = 0; i < STRESS_OBJS / 4; i++) {
        objs[i] = kmem_cache_alloc(cache);
    }
    printf("Re-allocated %d objects\n", STRESS_OBJS / 4);

    // Free all
    for(i = 0; i < STRESS_OBJS; i++) {
        if(objs[i]) {
            kmem_cache_free(cache, objs[i]);
        }
    }
    printf("PASS: Freed all objects\n");

    kmem_cache_info(cache);
    kmem_cache_destroy(cache);
    printf("PASS: Stress test completed\n\n");
}

void
test_edge_cases(void)
{
    printf("TEST 6: Edge Cases\n");
    printf("==================\n");

    // Test NULL handling
    kmem_cache_free(0, (void *)0x1000);
    printf("PASS: NULL cache handle handled\n");

    void *obj = kmem_cache_alloc(0);
    if(obj == 0) {
        printf("PASS: NULL cache alloc returns NULL\n");
    }

    // Test double free (should not crash)
    kmem_cache_t *cache = kmem_cache_create("edge", 64, 0, 0);
    obj = kmem_cache_alloc(cache);
    kmem_cache_free(cache, obj);
    // Double free - might cause error state but shouldn't crash
    kmem_cache_free(cache, obj);
    printf("PASS: Double free handled\n");

    kmem_cache_destroy(cache);
    printf("\n");
}

int
main(int argc, char *argv[])
{
    printf("\n");
    printf("========================================\n");
    printf("  SLAB ALLOCATOR TEST SUITE\n");
    printf("========================================\n");
    printf("\n");

    test_basic_cache();
    test_cache_growth();
    test_slab_alloc();
    test_multiple_caches();
    test_stress();
    test_edge_cases();

    printf("========================================\n");
    printf("  ALL TESTS COMPLETED\n");
    printf("========================================\n");
    printf("\n");

    exit(0);
}