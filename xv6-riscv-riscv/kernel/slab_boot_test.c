#include "buddy.h"
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "spinlock.h"
#include "proc.h"
#include "slab.h"

struct test_obj {
    int id;
    char data[56];
};

void
slab_test_basic(void)
{
    printf("TEST 1: Basic Cache Operations\n");
    printf("================================\n");

    kmem_cache_t *cache = kmem_cache_create("test_cache", sizeof(struct test_obj), 0, 0);
    if(cache == 0) {
        printf("FAIL: Could not create cache\n");
        return;
    }
    printf("PASS: Cache created\n");

    kmem_cache_info(cache);

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

    kmem_cache_info(cache);

    kmem_cache_free(cache, obj1);
    kmem_cache_free(cache, obj2);
    kmem_cache_free(cache, obj3);
    printf("PASS: Freed 3 objects\n");

    kmem_cache_destroy(cache);
    printf("PASS: Cache destroyed\n\n");
}

void
slab_test_growth(void)
{
    printf("TEST 2: Cache Growth and Shrink\n");
    printf("====================\n");

    kmem_cache_t *cache = kmem_cache_create("growth_test", 64, 0, 0);
    if(cache == 0) {
        printf("FAIL: Could not create cache\n");
        return;
    }

    #define NUM_OBJS 30
    void *objs1[NUM_OBJS];

    for(int i = 0; i < NUM_OBJS; i++) {
        objs1[i] = kmem_cache_alloc(cache);
        if(objs1[i] == 0) {
            printf("FAIL: Allocation failed at object %d\n", i);
            break;
        }
    }

    printf("PASS: Allocated %d objects\n", NUM_OBJS);
    kmem_cache_info(cache);

    void *objs2[NUM_OBJS];
    for(int i = 0; i < NUM_OBJS; i++) {
        objs2[i] = kmem_cache_alloc(cache);
        if(objs2[i] == 0) {
            printf("FAIL: Allocation failed at object %d\n", i);
            break;
        }
    }

    printf("PASS: Allocated %d objects\n", NUM_OBJS);
    kmem_cache_info(cache);

    for(int i = 0; i < NUM_OBJS; i++) kmem_cache_free(cache, objs1[i]);

    printf("PASS: Freed %d objects\n", NUM_OBJS);

    int freed = kmem_cache_shrink(cache);
    printf("Shrink freed %d blocks\n", freed);
    kmem_cache_info(cache);

    for(int i = 0; i < NUM_OBJS; i++) kmem_cache_free(cache, objs2[i]);

    printf("PASS: Freed %d objects\n", NUM_OBJS);

    freed = kmem_cache_shrink(cache);
    printf("Shrink freed %d blocks\n", freed);
    kmem_cache_info(cache);

    kmem_cache_destroy(cache);
    printf("PASS: Test completed\n\n");
}

void
slab_test_buffers(void)
{
    printf("TEST 3: Buffer allocation/freeing\n");
    printf("=============================\n");

    void *buf1 = kmalloc(50);
    void *buf2 = kmalloc(100);
    void *buf3 = kmalloc(1000);

    if(buf1 && buf2 && buf3) {
        printf("PASS: Allocated 3 buffers\n");

        char *cb1 = (char *)buf1;
        char *cb2 = (char *)buf2;
        char *cb3 = (char *)buf3;

        cb1[0] = 'A';
        cb2[0] = 'B';
        cb3[0] = 'C';

        if(cb1[0] == 'A' && cb2[0] == 'B' && cb3[0] == 'C') {
            printf("PASS: Buffers are usable\n");
        }

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
run_slab_tests(void)
{
    printf("\n");
    printf("========================================\n");
    printf("  RUNNING SLAB ALLOCATOR TESTS\n");
    printf("========================================\n");
    printf("\n");

    printf("Initial Buddy State:\n");
    pretty_print_buddy();
    buddy_print_info();

    slab_test_basic();
    printf("Buddy State after basic tests:\n");
    pretty_print_buddy();
    buddy_print_info();

    slab_test_growth();
    printf("Buddy State after growth tests:\n");
    pretty_print_buddy();
    buddy_print_info();

    slab_test_buffers();
    printf("Buddy State after buffer tests:\n");
    pretty_print_buddy();
    buddy_print_info();

    printf("========================================\n");
    printf("  SLAB ALLOCATOR TESTS COMPLETED\n");
    printf("========================================\n");
    printf("\n");
}