#include "buddy.h"
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "spinlock.h"

void run_buddy_tests() {
    buddy_test_alloc_free();
}

void buddy_test_alloc_free() {
    printf("TEST: Buddy Alloc/Free\n");
    printf("======================\n");
    
    pretty_print_buddy();

    printf("Allocating block of order 0...\n");
    void *ptr1 = buddy_alloc(1);

    printf("ptr1: %p\n", ptr1);
    pretty_print_buddy();

    printf("Allocating block of order 8...\n");
    void *ptr2 = buddy_alloc(8);

    printf("ptr2: %p\n", ptr2);
    pretty_print_buddy();

    printf("Allocating another block of order 8...\n");
    void *ptr3 = buddy_alloc(8);

    printf("ptr3: %p\n", ptr3);
    pretty_print_buddy();

    printf("Allocating another block of order 2...\n");
    void *ptr4 = buddy_alloc(2);

    printf("ptr4: %p\n", ptr4);
    pretty_print_buddy();

    printf("Freeing ptr1...\n");
    buddy_free(ptr1, 1);
    pretty_print_buddy();

    printf("Freeing ptr2...\n");
    buddy_free(ptr2, 8);
    pretty_print_buddy();

    printf("Freeing ptr3...\n");
    buddy_free(ptr3, 8);
    pretty_print_buddy();

    printf("Freeing ptr4...\n");
    buddy_free(ptr4, 2);
    pretty_print_buddy();
    
    printf("======================\n\n");
}

void buddy_test_coalesce() {
    printf("TEST: Buddy Coalescing\n");
    printf("======================\n");

    pretty_print_buddy();

    printf("Allocating two blocks of order 0...\n");
    void *ptr1 = buddy_alloc(1);
    void *ptr2 = buddy_alloc(1);

    printf("ptr1: %p, ptr2: %p\n", ptr1, ptr2);
    pretty_print_buddy();

    printf("Freeing ptr1...\n");
    buddy_free(ptr1, 1);
    pretty_print_buddy();

    printf("Freeing ptr2...\n");
    buddy_free(ptr2, 1);
    pretty_print_buddy();

    printf("======================\n\n");
}
