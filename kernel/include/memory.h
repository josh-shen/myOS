#ifndef _MEMORY_H
#define _MEMORY_H

#include <stdint.h>

/* Physical memory manager */
struct partition {
    struct partition *prev;
    struct partition *next;
};
typedef struct partition partition_t;

void pmm_init(uint32_t);
uint32_t pmm_alloc(void);
void pmm_free(uint32_t physical_address);

/* Virtual memory manager */
void vmm_map(uint32_t, uint32_t);

#endif