#ifndef _MEMORY_H
#define _MEMORY_H

#include <stdint.h>

/* Physical memory manager */
void pmm_init(uint32_t, uint32_t);
int pmm_alloc(uint32_t);
void pmm_free(uint32_t);

/* Virtual memory manager */
void vmm_init_kernel_space(uint32_t, uint32_t);

#endif