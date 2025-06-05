#ifndef _MEMORY_H
#define _MEMORY_H

#include <stdint.h>

/* Physical memory manager */
void pmm_init(void);
uint32_t pmm_alloc_frame(void);
void pmm_free_frame(uint32_t physical_address);

/* Virtual memory manager */

#endif