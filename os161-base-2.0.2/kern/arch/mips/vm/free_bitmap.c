#include <types.h>
#include <spinlock.h>
#include <lib.h>
#include <free_bitmap.h>

typedef enum { FREE, BUSY } state_t;

typedef state_t bit;

typedef struct bitmap {
    bit * next_bit;
    unsigned int length;
    struct spinlock bitmap_lock;
} bitmap_t;

static bitmap_t * free_pmem_bitmap; //pmem = physical memory

void bitmap_init(unsigned int length) {
    free_pmem_bitmap = (bitmap_t*) kmalloc(sizeof(bitmap_t));
    free_pmem_bitmap->next_bit = (bit*) kmalloc(sizeof(bit)*length);
    free_pmem_bitmap->length = length;
    spinlock_init(&free_pmem_bitmap->bitmap_lock);

    unsigned int i = 0;
    spinlock_acquire(&free_pmem_bitmap->bitmap_lock);
    for(i=0; i<length; i++){
        free_pmem_bitmap->next_bit[i] = FREE;
    }
    spinlock_release(&free_pmem_bitmap->bitmap_lock);
}

void bitmap_set_free(unsigned int i) {
    spinlock_acquire(&free_pmem_bitmap->bitmap_lock);
    free_pmem_bitmap->next_bit[i] = FREE;
    spinlock_release(&free_pmem_bitmap->bitmap_lock);
}

void bitmap_set_busy(unsigned int i) {
    spinlock_acquire(&free_pmem_bitmap->bitmap_lock);
    free_pmem_bitmap->next_bit[i] = BUSY;
    spinlock_release(&free_pmem_bitmap->bitmap_lock);
}

int bitmap_find_first_free(unsigned int * first_free) {
    unsigned int i = 0;
    int result = -1;
    spinlock_acquire(&free_pmem_bitmap->bitmap_lock);
    for(i=0; i<free_pmem_bitmap->length; i++){
        if(free_pmem_bitmap->next_bit[i] == FREE)
        {
            *first_free = i;
            result = 0;
        }        
    }
    spinlock_release(&free_pmem_bitmap->bitmap_lock);
    return result;
}

void bitmap_destroy_mem(void) {
    unsigned int i = 0;
    spinlock_acquire(&free_pmem_bitmap->bitmap_lock);
    for(i=0; i<free_pmem_bitmap->length; i++) {
        kfree(&free_pmem_bitmap->next_bit[i]);
    }
    spinlock_release(&free_pmem_bitmap->bitmap_lock);
    kfree(free_pmem_bitmap);
}