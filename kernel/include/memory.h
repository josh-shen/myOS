#ifndef _MEMORY_H
#define _MEMORY_H

#include <stdint.h>
#include <stddef.h>

/* Physical memory manager, buddy allocator */
#define MEM_BLOCK_LOG2 27
#define MAX_BLOCK_LOG2 22
#define MIN_BLOCK_LOG2 12
#define MAX_ORDER (MAX_BLOCK_LOG2 - MIN_BLOCK_LOG2)

#define TOTAL_TREE_NODES ((1 << (MEM_BLOCK_LOG2 - MIN_BLOCK_LOG2 + 1)) - 1)
#define TRUNCATED_TREE_NODES ((1 << (MEM_BLOCK_LOG2 - MAX_BLOCK_LOG2)) - 1)
#define TREE_NODES (TOTAL_TREE_NODES - TRUNCATED_TREE_NODES)
#define TREE_WORDS ((TREE_NODES + 31) / 32)

struct buddy_block {
    struct buddy_block *prev;
    struct buddy_block *next;
};
typedef struct buddy_block buddy_block_t;

struct buddy {
    uint32_t base;
    uint32_t size;
    uint32_t bit_tree[TREE_WORDS];
    buddy_block_t *free_lists[MAX_ORDER + 1];

};
typedef struct buddy buddy_t;

uint32_t pmm_init(uint32_t, uint32_t);
uint32_t pmm_malloc(uint32_t);
void pmm_free(uint32_t, uint32_t);

/* Virtual memory manager */
typedef enum {
    PTE_PRESENT         = 0x1,
    PTE_READ_WRITE      = 0x2,
    PTE_USER_SUPERVISOR = 0x4,
    PTE_WRITETHROUGH    = 0x8,
    PTE_CACHE_DISABLE   = 0x10,
    PTE_ACCESSED        = 0x20,
    PTE_DIRTY           = 0x40,
    PTE_PAT             = 0x80,
    PTE_GLOBAL          = 0x100,
    PTE_AVAILABLE       = 0xE00,
    PTE_FRAME           = 0xFFFFF000
} PAGE_TABLE_FLAGS;

typedef enum {
    PDE_PRESENT         = 0x1,
    PDE_READ_WRITE      = 0x2,
    PDE_USER_SUPERVISOR = 0x4,
    PDE_WRITETHROUGH    = 0x8,
    PDE_CACHE_DISABLE   = 0x10,
    PDE_ACCESSED        = 0x20,
    PDE_PAGE_SIZE       = 0x40,
    PDE_AVAILABLE       = 0xF00,
    PDE_FRAME           = 0xFFFFF000
} PAGE_DIRECTORY_FLAGS;

struct page_table {
    uint32_t entries[1024];
}; typedef struct page_table page_table_t;

struct page_directory {
    uint32_t entries[1024];
}; typedef struct page_directory page_directory_t;

struct vm_area {
    struct vm_area *next;
    uint32_t addr;
    uint32_t size;
    uint8_t used;
}; typedef struct vm_area vm_area_t;

void vmm_init(uint32_t);
void vmm_map(uint32_t, uint32_t, uint32_t);
uint32_t vmm_unmap(uint32_t);
uint32_t vmm_malloc(uint32_t);
void vmm_free(uint32_t, uint32_t);

/* Kernel memory, slab allocator */
struct object {
    struct object *next;
};
typedef struct object object_t;

struct slab {
    struct object *head;
    uint32_t inuse;
};
typedef struct slab slab_t;

struct cache {
    struct cache *next;
    uint32_t objsize;
    uint32_t num; // number of objects per slab
    // flags, spinlock

    slab_t *slabs_full;
    slab_t *slabs_partial;
    slab_t *slabs_empty;
};
typedef struct cache cache_t;

void kmem_init();
uint32_t kmalloc(uint32_t);
void kfree(void *);

#endif