#include "../kernel/types.h"
#include "../kernel/stat.h"
#include "user.h"

struct test_obj {
    int val;
};

void ctor(void *obj) {
    struct test_obj *t = (struct test_obj *)obj;
    t->val = 42;
}

void main() {
    kmem_cache_t *cache = kmem_cache_create("ctortest", sizeof(struct test_obj), ctor, 0);
    if (cache == 0) {
        printf("FAIL: cache_create\n");
        exit(1);
    }

    struct test_obj *obj = (struct test_obj *)kmem_cache_alloc(cache);
    if (obj == 0) {
        printf("FAIL: cache_alloc\n");
        exit(1);
    }

    if (obj->val == 42) {
        printf("PASS: constructor called, val=42\n");
    } else {
        printf("FAIL: constructor NOT called, val=%d\n", obj->val);
    }

    kmem_cache_free(cache, obj);
    kmem_cache_destroy(cache);
    exit(0);
}
