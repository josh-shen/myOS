#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <memory.h>
#include <multiboot.h>

static void mark_free(uint64_t, uint64_t);
static void mark_used(uint64_t, uint64_t);

extern char kernel_start;
extern char kernel_end;

#define FRAME_SIZE 4096             // 4 KiB pages
#define TOTAL_FRAMES 1048576        // 4 GiB memory

uint8_t pmm_bitmap[TOTAL_FRAMES / 8]__attribute__((section(".pmm_bitmap")));
struct physical_memory_manager {
    uint32_t total_frames;
    uint32_t free_frames;
};
typedef struct physical_memory_manager pmm_t;

static pmm_t pmm;

static void mark_free(uint64_t base, uint64_t len) {
    for (uint64_t curr_address = base; curr_address < base + len; curr_address += FRAME_SIZE) {
        uint16_t frame_num = curr_address / FRAME_SIZE;
        uint16_t byte_index = frame_num / 8;
        uint8_t bit_index = frame_num % 8;

        pmm_bitmap[byte_index] &= ~(1 << bit_index);
        pmm.free_frames += 1;
    }
}

static void mark_used(uint64_t base, uint64_t len) {
    for (uint64_t curr_address = base; curr_address < base + len; curr_address += FRAME_SIZE) {
        uint16_t frame_num = curr_address / FRAME_SIZE;
        uint16_t byte_index = frame_num / 8;
        uint8_t bit_index = frame_num % 8;

        pmm_bitmap[byte_index] |= (1 << bit_index);
    }
}

void pmm_init(uint32_t multiboot_info_ptr) {
    memset(pmm_bitmap, 0xFF, sizeof(pmm_bitmap));
    pmm.total_frames = TOTAL_FRAMES;
    pmm.free_frames = 0;
    
    multiboot_info_t *mbi = (multiboot_info_t*)multiboot_info_ptr;

    // Check if bit 6 of flags is set for mmap_*
    if (!(mbi->flags & (1 << 6))) {
        printf("Memory map not available\n");
        return;
    }

    // Pointer to first memory map entry
    mmap_entry_t *mmap_entry = (mmap_entry_t *)mbi->mmap_addr;

    while((uintptr_t)mmap_entry < mbi->mmap_addr + mbi->mmap_length) {
        uint64_t base_addr = ((uint64_t)mmap_entry->base_addr_high << 32) | mmap_entry->base_addr_low;
        uint64_t length = ((uint64_t)mmap_entry->length_high << 32) | mmap_entry->length_low;
        printf("%x, %x, %x\n",mmap_entry->base_addr_low, mmap_entry->length_low, mmap_entry->type);
        if (mmap_entry->type == 1) {
            printf("free\n");
            mark_free(base_addr, length);
        } else if (mmap_entry->type == 3) {
            // TODO: This is ACPI reclimable memory, ACPI data needs to be processed first
            mark_free(base_addr, length);
        } else if ((mmap_entry->type == 2) || (mmap_entry->type == 4) || (mmap_entry->type == 5)) {
            printf("used\n");
            mark_used(base_addr, length);
        }

        // Add memory map entry size + size field
        mmap_entry = (mmap_entry_t *)((uintptr_t)mmap_entry + mmap_entry->size + sizeof(mmap_entry->size));
    }

    mark_used((uintptr_t)&kernel_start, (uintptr_t)&kernel_end - (uintptr_t)&kernel_start); // Kernel, includes kernel stack and PMM bitmap
    mark_used((uintptr_t)multiboot_info_ptr, sizeof(multiboot_info_t));                     // MBI struct
    mark_used(mbi->mmap_addr, mbi->mmap_length);                                            // Memory map buffer
    mark_used(0xB80000, 8000);                                                              // VGA memory
    // TODO: Mark areas used by boot modules (mods_*)
    
    // TODO: Unmap the identiy mapping of the first 4 MiB of memory
}

uint32_t pmm_alloc() {

}

void pmm_free(uint32_t physical_address) {

}