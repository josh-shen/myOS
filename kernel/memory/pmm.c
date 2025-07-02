#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#include <memory.h>
#include <multiboot.h>

static void mark_free(uintptr_t, uintptr_t);
static int get_order(uintptr_t);
static int split_partition(uint8_t, uint8_t);

extern char kernel_start;
extern char kernel_len;

// Known used regions 
#define NUM_USED_REGIONS 2
static uintptr_t used_regions[NUM_USED_REGIONS][2] = {
    {(uintptr_t)&kernel_start, (uintptr_t)&kernel_len}, // Kernel
    {0xB80000, 8000}                                    // VGA memory
};

// Free list linked list structure
struct partition {
    struct partition *prev;
    struct partition *next;
};
typedef struct partition partition_t;

#define MAX_ORDER 11
static partition_t *free_lists[MAX_ORDER]__attribute__((section(".free_lists")));

static int get_order(uintptr_t length) {
    if (length > 2 << (MAX_ORDER + 11)) return -1;

    for (int n = MAX_ORDER - 1; n >= 0; n--) {
        if ((uintptr_t)(2 << (n + 11)) <= length) return n;
    }
    return 0;
}

static void mark_free(uintptr_t base, uintptr_t length) {
    if (length == 0) return;
    // Filter out used regions
    for (int i = 0; i < NUM_USED_REGIONS; i++) {
        uintptr_t used_end = used_regions[i][0] + used_regions[i][1];
        uintptr_t end = base + length;

        if (used_regions[i][0] >= base && used_end <= end) {
            mark_free(base, used_regions[i][0] - base);
            mark_free(used_end, end - used_end);
            return;
        }
    }

    vmm_init_kernel_space(base, length);
    
    // Add memory to free lists to mark as free
    while (length >= 4096) {
        int order = get_order(length);
        uint32_t partition_size = 2 << (order + 11);
        
        partition_t *partition = (partition_t *)(uintptr_t)(base + 0xC0000000);        
        if (free_lists[order] == NULL) {
            partition->prev = NULL;
            partition->next = NULL;
            free_lists[order] = partition;
        } else {
            partition->next = free_lists[order];
            partition->prev = NULL;

            free_lists[order]->prev = partition;
            free_lists[order] = partition;
        }

        base += partition_size;
        length -= partition_size;
    }
}

static int split_partition(uint8_t index, uint8_t target) {
    while (index > target || index >= 0) {
        partition_t *partition = free_lists[index];

        uint32_t address = (uint32_t)((uintptr_t)partition);
        uint32_t buddy_address = address + ((2 << (index + 11)) / 2);

        // Remove the partition from the free list
        free_lists[index] = partition->next;
        free_lists[index]->prev = NULL;

        // Create buddy partition
        partition_t *buddy_partition = (partition_t *)(uintptr_t)(buddy_address);

        // Add both buddies to their new free list
        partition->prev = NULL;
        partition->next = buddy_partition;

        buddy_partition->prev = partition;
        buddy_partition->next = NULL;

        free_lists[index--] = partition;
        
        if (index == target || index == 0) {
            return index;
        }

    }

    return -1;
}

void pmm_init(uint32_t mmap_addr, uint32_t mmap_length) {    
    // Initialize free lists
    for (uint8_t i = 0; i < MAX_ORDER; i++) {
        free_lists[i] = NULL;
    }

    // Pointer to first memory map entry
    mmap_entry_t *mmap_entry = (mmap_entry_t *)mmap_addr;

    while((uintptr_t)mmap_entry < mmap_addr + mmap_length) {
        uintptr_t base_addr = ((uint64_t)mmap_entry->base_addr_high << 32) | mmap_entry->base_addr_low;
        uintptr_t length = ((uint64_t)mmap_entry->length_high << 32) | mmap_entry->length_low;

        if (mmap_entry->type == 1) {
            mark_free(base_addr, length);
        } else if (mmap_entry->type == 3) {
            // TODO: This is ACPI reclimable memory, ACPI data needs to be processed first
            mark_free(base_addr, length);
        }

        // Add memory map entry size + size field
        mmap_entry = (mmap_entry_t *)((uintptr_t)mmap_entry + mmap_entry->size + sizeof(mmap_entry->size));
    }
    
    // TODO: Mark areas used by boot modules (mods_*), okther multiboot info needed
    
    // TODO: Unmap the identiy mapping of the first 4 MiB of memory
}

int pmm_alloc(uint32_t size) {
    int order = get_order(size);

    if (order == -1) {
        printf("Requested size is too large: %d bytes\n", size);
        return -1; // Size too large
    }

    if (free_lists[order] != NULL) {
        partition_t *partition = free_lists[order];

        // Remove the partition from the free list
        // TODO: handle case where partition is the only one in the list
        free_lists[order] = partition->next;
        free_lists[order]->prev = NULL;

        return (uint32_t)((uintptr_t)partition - 0xC0000000); // Return physical address
    } else {
        // Search for a larger partition to split
        for (int i = order + 1; i < MAX_ORDER; i++) {
            if (free_lists[i] != NULL) {
                // A larger partition found, split
                int index = split_partition(i, order);

                if (index == -1) {
                    printf("Failed to split partition\n");
                    return -1;
                }

                partition_t *partition = free_lists[index];
                free_lists[index] = partition->next;
                free_lists[index]->prev = NULL;

                return (uint32_t)((uintptr_t)partition - 0xC0000000); // Return physical address
            }
        }

        return -1; // No free partition found
    }
}

void pmm_free(uint32_t physical_address) {
    /*
    return the partition to the free list

    find the order of the partition

    if - the partition's buddy is also free
        coalesce the two partitions into one
        continue merging if possible
        add the partition to the free list
    else
        add the partition to the free list
    */
}