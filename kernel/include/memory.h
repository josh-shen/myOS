#ifndef _MEMORY_H
#define _MEMORY_H

#include <stdint.h>

/* Physical memory manager */
void pmm_init(uint32_t, uint32_t);
uint32_t pmm_alloc(void);
void pmm_free(uint32_t physical_address);

/* Virtual memory manager */
void vmm_init_kernel_space(uint32_t, uint32_t);

#endif