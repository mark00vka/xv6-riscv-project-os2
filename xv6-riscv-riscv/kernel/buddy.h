#ifndef _BUDDY_H_
#define _BUDDY_H_

#include "types.h"
#include "spinlock.h"
#define BUDDY_MAX_ORDER 10  // 2^10 = 1024 blokova od po 4KB = 4MB
#define BUDDY_MAX_BLOCKS (1 << BUDDY_MAX_ORDER)

// Struktura za listu slobodnih blokova određenog reda
struct buddy_free_area {
    struct list_head free_list;
    int nr_free;
};

// Glavna struktura Buddy sistema
struct buddy_system {
    void *memory_start;         // početak memorije kojom upravlja
    int total_blocks;           // ukupan broj blokova (stepen dvojke)
    int max_order;              // maksimalni red (log2(total_blocks))

    // Bitmap za praćenje stanja blokova (0-slobodan, 1-zauzet)
    // Koristimo jedan bit po minimalnom bloku
    uint32 *bitmap;
    int bitmap_blocks;          // koliko blokova zauzima bitmap

    // Niz listi slobodnih blokova po redovima
    struct buddy_free_area free_area[BUDDY_MAX_ORDER + 1];

    struct spinlock lock;
};

// Funkcije Buddy Allocatora
void buddy_init(struct buddy_system *buddy, void *space, int block_num);
void *buddy_alloc(struct buddy_system *buddy, int order);
void buddy_free(struct buddy_system *buddy, void *addr, int order);
int buddy_get_order(struct buddy_system *buddy, int block_idx);
void buddy_print_info(struct buddy_system *buddy);

// Pomoćne funkcije
static inline int is_power_of_two(int n) {
    return (n & (n - 1)) == 0;
}

static inline int round_up_pow2(int n) {
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n++;
    return n;
}

static inline int log2_int(int n) {
    int r = 0;
    while (n >>= 1) r++;
    return r;
}

#endif // _BUDDY_H_