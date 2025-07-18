#ifndef _MEMORY_H
#define _MEMORY_H

#include <stdint.h>

//#define LOGGING
#define ERR_LOGGING

/* Physical memory manager */
#define MEM_BLOCK_LOG2 27
#define MAX_BLOCK_LOG2 22
#define MIN_BLOCK_LOG2 12
#define MAX_ORDER (MAX_BLOCK_LOG2 - MIN_BLOCK_LOG2)

#define TOTAL_TREE_NODES ((1 << (MEM_BLOCK_LOG2 - MIN_BLOCK_LOG2 + 1)) - 1)
#define TRUNCATED_TREE_NODES ((1 << (MEM_BLOCK_LOG2 - MAX_BLOCK_LOG2)) - 1)
#define TREE_NODES (TOTAL_TREE_NODES - TRUNCATED_TREE_NODES)
#define TREE_WORDS ((TREE_NODES + 31) / 32)

struct buddy_page {
    struct buddy_page *prev;
    struct buddy_page *next;
};
typedef struct buddy_page buddy_page_t;

struct buddy {
    uintptr_t base;
    uintptr_t size;
    uint32_t bit_tree[TREE_WORDS];
    buddy_page_t *free_lists[MAX_ORDER + 1];

};
typedef struct buddy buddy_t;

void pmm_init(uint32_t, uint32_t);
uint32_t pmm_malloc(uint32_t);
void pmm_free(uint32_t, uint32_t);

/* Virtual memory manager */
void vmm_init_kernel_space(uint32_t, uint32_t);

#endif