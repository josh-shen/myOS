#include <stdio.h>
#include <stdint.h>

#include <memory.h>
#include <multiboot.h>

static void mark_free(uint64_t, uint64_t);
static void mark_used(uint64_t, uint64_t);

extern multiboot_info_t *multiboot_info_ptr;

#define FRAME_SIZE 4096                     // 4 KB pages
#define MAX_FRAMES 32752                    // Total frames of memory
uint8_t pmm_bitmap[MAX_FRAMES / 8] = {1};   // Each bit of a byte represents a frame

static void mark_free(uint64_t base, uint64_t len) {
    for (uint64_t curr_frame = base; curr_frame < base + len; curr_frame += FRAME_SIZE) {
        uint16_t frame_num = curr_frame / 4096;
        uint16_t byte_index = frame_num / 8;
        uint8_t bit_index = frame_num % 8;

        pmm_bitmap[byte_index] &= ~(1 << bit_index);
    }
}

static void mark_used(uint64_t base, uint64_t len) {
    for (uint64_t curr_frame = base; curr_frame < base + len; curr_frame += FRAME_SIZE) {
        uint16_t frame_num = curr_frame / 4096;
        uint16_t byte_index = frame_num / 8;
        uint8_t bit_index = frame_num % 8;

        pmm_bitmap[byte_index] |= (1 << bit_index);
    }
}

void init_memory() {
    multiboot_info_t *mbi = multiboot_info_ptr;
	    
    // Check if bit 6 of flags is set
    if (!(mbi->flags & (1 << 6))) {
        printf("Memory map not available\n");
        return;
    }

    // Pointer to first memory map entry
    mmap_entry_t *mmap_entry = (mmap_entry_t *)mbi->mmap_addr;

    while((uintptr_t)mmap_entry < mbi->mmap_addr + mbi->mmap_length) {
        uint64_t base_addr = ((uint64_t)mmap_entry->base_addr_high << 32) | mmap_entry->base_addr_low;
        uint64_t length = ((uint64_t)mmap_entry->length_high << 32) | mmap_entry->length_low;
        
        if (mmap_entry->type == 1) {
            mark_free(base_addr, length);
        } else if (mmap_entry->type == 3) {
            // TODO: This is ACPI reclimable memory, ACPI data needs to be processed first
            mark_free(base_addr, length);
        } else if ((mmap_entry->type == 2) || (mmap_entry->type == 4) || (mmap_entry->type == 5)) {
            mark_used(base_addr, length);
        }

        // Add memory map entry size + size field
        mmap_entry = (mmap_entry_t *)((uintptr_t)mmap_entry + mmap_entry->size + sizeof(mmap_entry->size));
    }

    // bitmap size is 4094 bytes (32752 / 8), < 4KB. The first frame of the bitmap goes to the bitmap itself
}