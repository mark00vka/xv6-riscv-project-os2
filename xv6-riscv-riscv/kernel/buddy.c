#include "buddy.h"
#include "string.h"
#include "printf.h"

// Pomocne funkcije za rad sa bitmap-om
static void set_bit(struct buddy_system *buddy, int bit) {
    int word = bit / 32;
    int offset = bit % 32;
    buddy->bitmap[word] |= (1U << offset);
}

static void clear_bit(struct buddy_system *buddy, int bit) {
    int word = bit / 32;
    int offset = bit % 32;
    buddy->bitmap[word] &= ~(1U << offset);
}

static int test_bit(struct buddy_system *buddy, int bit) {
    int word = bit / 32;
    int offset = bit % 32;
    return (buddy->bitmap[word] >> offset) & 1U;
}

// Pronalazi indeks parnjaka za dati blok i red
static int find_buddy_index(struct buddy_system *buddy, int block_idx, int order) {
    return block_idx ^ (1 << order);
}

// Inicijalizacija Buddy sistema
void buddy_init(struct buddy_system *buddy, void *space, int block_num) {
    initlock(&buddy->lock, "buddy");

    // Zaokruži block_num na prvi veći stepen dvojke
    if (!is_power_of_two(block_num)) {
        block_num = round_up_pow2(block_num);
    }

    buddy->max_order = log2_int(block_num);
    if (buddy->max_order > BUDDY_MAX_ORDER) {
        buddy->max_order = BUDDY_MAX_ORDER;
        block_num = 1 << BUDDY_MAX_ORDER;
    }

    buddy->total_blocks = block_num;

    // Izračunaj veličinu bitmap-e (u bitovima = block_num)
    // bitmap zauzima (block_num + 31) / 32 * 4 bajtova
    int bitmap_size = ((block_num + 31) / 32) * sizeof(uint32);
    buddy->bitmap_blocks = (bitmap_size + BLOCK_SIZE - 1) / BLOCK_SIZE;

    // Bitmap se nalazi na početku dodeljene memorije
    buddy->bitmap = (uint32*)space;

    // Memorija za blokove počinje posle bitmap-e
    buddy->memory_start = (char*)space + buddy->bitmap_blocks * BLOCK_SIZE;

    // Inicijalizuj liste za sve redove
    for (int i = 0; i <= buddy->max_order; i++) {
        initlock(&buddy->free_area[i].lock, "free_area");
        init_list_head(&buddy->free_area[i].free_list);
        buddy->free_area[i].nr_free = 0;
    }

    // Inicijalno, svi blokovi su slobodni
    // Postavi sve bitove u bitmap-i na 0 (slobodno)
    memset(buddy->bitmap, 0, bitmap_size);

    // Dodaj celokupan prostor (najveći red) u listu slobodnih
    struct list_head *free_list = &buddy->free_area[buddy->max_order].free_list;

    // Kreiraj strukturu za slobodan blok (koristi prvi blok za čuvanje strukture)
    // Ovo je pojednostavljena implementacija - u praksi bi se koristila posebna struktura
    struct free_block {
        struct list_head list;
        int block_idx;
    };

    // Dodaj prvi (i jedini) slobodan blok maksimalne veličine
    struct free_block *fb = (struct free_block*)buddy->memory_start;
    fb->block_idx = 0;
    init_list_head(&fb->list);
    list_add(&fb->list, free_list);
    buddy->free_area[buddy->max_order].nr_free++;

    printf("Buddy init: %d blocks, max_order=%d, bitmap=%d blocks\n",
           block_num, buddy->max_order, buddy->bitmap_blocks);
}

// Alokacija 2^order blokova
void *buddy_alloc(struct buddy_system *buddy, int order) {
    if (order > buddy->max_order) {
        return 0;
    }

    acquire(&buddy->lock);

    // Traži slobodan blok odgovarajuće veličine
    int curr_order = order;
    while (curr_order <= buddy->max_order &&
           buddy->free_area[curr_order].nr_free == 0) {
        curr_order++;
    }

    if (curr_order > buddy->max_order) {
        // Nema slobodne memorije
        release(&buddy->lock);
        return 0;
    }

    // Uzmi prvi slobodan blok iz liste
    struct list_head *free_list = &buddy->free_area[curr_order].free_list;
    struct free_block *fb = list_first_entry(free_list, struct free_block, list);
    int block_idx = fb->block_idx;

    // Ukloni iz liste
    list_del(&fb->list);
    buddy->free_area[curr_order].nr_free--;

    // Podeli blok dok ne dođemo do željenog reda
    while (curr_order > order) {
        curr_order--;

        // Kreiraj parnjaka
        int buddy_idx = find_buddy_index(buddy, block_idx, curr_order);

        // Dodaj parnjaka u listu slobodnih
        struct free_block *buddy_fb = (struct free_block*)(
            (char*)buddy->memory_start + buddy_idx * BLOCK_SIZE * (1 << curr_order)
        );
        buddy_fb->block_idx = buddy_idx;
        init_list_head(&buddy_fb->list);

        list_add(&buddy_fb->list, &buddy->free_area[curr_order].free_list);
        buddy->free_area[curr_order].nr_free++;
    }

    // Označi blok kao zauzet u bitmap-i
    int start_bit = block_idx * (1 << order);
    int bits = 1 << order;
    for (int i = 0; i < bits; i++) {
        set_bit(buddy, start_bit + i);
    }

    release(&buddy->lock);

    // Vrati adresu alociranog bloka
    return (char*)buddy->memory_start + block_idx * BLOCK_SIZE * (1 << order);
}

// Dealokacija bloka
void buddy_free(struct buddy_system *buddy, void *addr, int order) {
    if (!addr || order > buddy->max_order) {
        return;
    }

    // Izračunaj indeks bloka
    uint64 offset = (char*)addr - (char*)buddy->memory_start;
    int block_idx = offset / (BLOCK_SIZE * (1 << order));

    acquire(&buddy->lock);

    // Oslobodi blok u bitmap-i
    int start_bit = block_idx * (1 << order);
    int bits = 1 << order;
    for (int i = 0; i < bits; i++) {
        clear_bit(buddy, start_bit + i);
    }

    // Pokušaj da spojiš sa parnjakom
    while (order < buddy->max_order) {
        int buddy_idx = find_buddy_index(buddy, block_idx, order);

        // Proveri da li je parnjak potpuno slobodan
        int buddy_start_bit = buddy_idx * (1 << order);
        int buddy_free = 1;

        for (int i = 0; i < (1 << order); i++) {
            if (test_bit(buddy, buddy_start_bit + i)) {
                buddy_free = 0;
                break;
            }
        }

        if (!buddy_free) {
            break;
        }

        // Ukloni parnjaka iz liste slobodnih
        struct list_head *list = &buddy->free_area[order].free_list;
        struct free_block *pos, *n;

        list_for_each_entry_safe(pos, n, list, list) {
            if (pos->block_idx == buddy_idx) {
                list_del(&pos->list);
                buddy->free_area[order].nr_free--;
                break;
            }
        }

        // Spoji blokove (koristi niži indeks)
        if (buddy_idx < block_idx) {
            block_idx = buddy_idx;
        }

        order++;
    }

    // Dodaj (spojeni) blok u listu slobodnih
    struct free_block *fb = (struct free_block*)(
        (char*)buddy->memory_start + block_idx * BLOCK_SIZE * (1 << order)
    );
    fb->block_idx = block_idx;
    init_list_head(&fb->list);

    list_add(&fb->list, &buddy->free_area[order].free_list);
    buddy->free_area[order].nr_free++;

    release(&buddy->lock);
}

// Ispis informacija o Buddy sistemu
void buddy_print_info(struct buddy_system *buddy) {
    acquire(&buddy->lock);

    printf("Buddy System Info:\n");
    printf("  Memory start: %p\n", buddy->memory_start);
    printf("  Total blocks: %d\n", buddy->total_blocks);
    printf("  Max order: %d\n", buddy->max_order);
    printf("  Bitmap blocks: %d\n", buddy->bitmap_blocks);

    for (int i = 0; i <= buddy->max_order; i++) {
        if (buddy->free_area[i].nr_free > 0) {
            printf("  Order %d (%d blocks): %d free\n",
                   i, (1 << i), buddy->free_area[i].nr_free);
        }
    }

    release(&buddy->lock);
}