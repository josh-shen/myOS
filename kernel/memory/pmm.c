#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#include <memory.h>
#include <multiboot.h>

static uint32_t round_pow2(uint32_t);
static uint8_t get_order(uint32_t);
static uint32_t get_bit_tree_index(uint32_t, uint8_t);
static uint8_t get_state(uint32_t, uint8_t);
static void set_state(uint32_t, uint8_t, uint8_t);
static void free_list_append(uint32_t, uint8_t);
static void free_list_remove(uint32_t, uint8_t);
static uint8_t split(uint8_t, uint8_t);
static void mark_free(uint32_t, uint32_t);

extern char kernel_start;
extern char kernel_len;

// Known used regions 
#define NUM_USED_REGIONS 2
static uintptr_t used_regions[NUM_USED_REGIONS][2] = {
    {(uintptr_t)&kernel_start, (uintptr_t)&kernel_len}, // Kernel
    {0xB80000, 8000}                                    // VGA memory
};

buddy_t alloc __attribute__((section(".buddy_allocator")));

static uint32_t round_pow2(uint32_t length) {
    length--;
    length |= length >> 1;
    length |= length >> 2;
    length |= length >> 4;
    length |= length >> 8;
    length |= length >> 16;
    length++;

    return length;
}

static uint8_t get_order(uint32_t length) {
    for (int n = MAX_BLOCK_LOG2; n >= MIN_BLOCK_LOG2; n--) {
        if ((unsigned)(1 << n) <= length) return n - MIN_BLOCK_LOG2;
    }
    return 0;
}

static uint32_t get_bit_tree_index(uint32_t address, uint8_t order) {
    uint8_t height = MEM_BLOCK_LOG2 - order - MIN_BLOCK_LOG2;
    uint32_t offset = (address - alloc.base) / (1 << (MIN_BLOCK_LOG2 + order));
    uint32_t node_index = (1 << height) - 1 + offset - TRUNCATED_TREE_NODES;

    return node_index;
}

static uint8_t get_state(uint32_t address, uint8_t order) {
    uint32_t index = get_bit_tree_index(address, order);
    uint32_t word_index = index / 32;
    uint32_t word_offset = index % 32;

    uint8_t state = alloc.bit_tree[word_index] >> word_offset;

    // Apply mask to get the desired bit
    return state & 1;
}

static void set_state(uint32_t address, uint8_t order, uint8_t state) {
    uint32_t index = get_bit_tree_index(address, order);
    uint32_t word_index = index / 32;
    uint32_t word_offset = index % 32;

    // Create a mask to only set the bit at the target position
    uint32_t mask = ~(1 << word_offset);

    alloc.bit_tree[word_index] = (alloc.bit_tree[word_index] & mask) | state << word_offset;
}

static void free_list_append(uint32_t address, uint8_t order) {
    buddy_block_t *block = (buddy_block_t *)(address + 0xC0000000);

    if (alloc.free_lists[order]) alloc.free_lists[order]->prev = block;

    block->prev = NULL;
    block->next = alloc.free_lists[order];

    alloc.free_lists[order] = block;
}

static void free_list_remove(uint32_t address, uint8_t order) {
    buddy_block_t *block = (buddy_block_t *)(address + 0xC0000000);

    if (block->prev != NULL) block->prev->next = block->next;

    if (alloc.free_lists[order] == block) alloc.free_lists[order] = block->next;

    if (block->next != NULL) block->next->prev = block->prev;

    block->prev = NULL;
    block->next = NULL;
}

static uint8_t split(uint8_t order, uint8_t target) {
    while (order > target) {
        buddy_block_t *block = alloc.free_lists[order];

        uint32_t address = (uint32_t)block - 0xC0000000;
        uint32_t buddy_address = ((address - alloc.base) ^ 1 << (order - 1 + MIN_BLOCK_LOG2)) + alloc.base;

        free_list_remove(address, order);

        // Mark the parent block as split in the bit tree
        set_state(address, order, 1);

        order--;

        // Add both buddies to free lists and mark them as free in the bit tree
        free_list_append(address, order);
        set_state(address, order, 0);

        free_list_append(buddy_address, order);
        set_state(buddy_address, order, 0);

        if (order == target) return order;
    }
    return MAX_ORDER + 1;
}

static void mark_free(uint32_t base, uint32_t length) {
    if (length == 0) return;
    // Filter out used regions
    for (int i = 0; i < NUM_USED_REGIONS; i++) {
        uint32_t used_end = used_regions[i][0] + used_regions[i][1];

        uint32_t end = base + length;

        if (used_regions[i][0] >= base && used_end <= end) {
            mark_free(base, used_regions[i][0] - base);
            mark_free(used_end, end - used_end);
            return;
        }
    }

    // Create linear mappings of addresses to address + 0xC0000000
    uint32_t addr = base;
    uint32_t unmapped_length = length;
    
    while (unmapped_length >= 4096) {
        vmm_map(addr + 0xC0000000, addr, 0x3);

        addr += 4096;
        unmapped_length -= 4096; 
    }
    
    // Add memory to free lists to mark as free
    while (length >= 4096) {
        uint8_t order = get_order(length);

        uint32_t block_size = 2 << (order + 11);
        
        free_list_append(base, order);

        set_state(base, order, 0);

        base += block_size;
        length -= block_size;
    }
}

uint32_t pmm_init(uint32_t mmap_addr, uint32_t mmap_length) {
    alloc.base = 0x0; 
    alloc.size = 0x8000000; 

    // Make sure length of used regions are a power of 2, if not round it up
    for (int i = 0; i < NUM_USED_REGIONS; i++) {
        if ((used_regions[i][1] & (used_regions[i][1] - 1)) != 0) {
            used_regions[i][1] = round_pow2(used_regions[i][1]);
        }
    }

    // Initialize bit tree - all bits initially set to 1, only free blocks will be changed to 0
    for (int i = 0; i < TREE_WORDS; i++) {
        alloc.bit_tree[i] = 0xFFFFFFFF;
    }

    // Initialize free lists
    for (int i = 0; i <= MAX_ORDER; i++) {
        alloc.free_lists[i] = NULL;
    }

    // Pointer to first memory map entry
    mmap_entry_t *mmap_entry = (mmap_entry_t *)mmap_addr;

    uint32_t addr_end;

    while((uint32_t)mmap_entry < mmap_addr + mmap_length) {
        uintptr_t base_addr = ((uint64_t)mmap_entry->base_addr_high << 32) | mmap_entry->base_addr_low;
        uintptr_t length = ((uint64_t)mmap_entry->length_high << 32) | mmap_entry->length_low;

        if (mmap_entry->type == 1) {
            mark_free(base_addr, length);
            addr_end = base_addr + length;
        } else if (mmap_entry->type == 3) {
            // TODO: This is ACPI reclimable memory, ACPI data needs to be processed first
            mark_free(base_addr, length);
            addr_end = base_addr + length;
        }

        // Add memory map entry size + size field
        mmap_entry = (mmap_entry_t *)((uint32_t)mmap_entry + mmap_entry->size + sizeof(mmap_entry->size));
    }

    // TODO: Mark areas used by boot modules (mods_*), okther multiboot info needed
    
    // TODO: Unmap the identiy mapping of the first 4 MiB of memory
    
    // Return last used virtual address used for linear mappings with a one page gap
    return addr_end + 0xC0001000;
}

uint32_t pmm_malloc(uint32_t length) {
    if (length > 1 << MAX_BLOCK_LOG2) {
        return 0;
    }

    uint8_t order = get_order(length);

    // Block of the requested order is available - no need to split
    if (alloc.free_lists[order] != NULL) {
        buddy_block_t *block_size = alloc.free_lists[order];

        uint32_t address = (uint32_t)block_size - 0xC0000000;

        free_list_remove(address, order);

        // Mark block as used in the bit tree
        set_state(address, order, 1);

        return address;
    }

    // A best fit block was not available - search for a larger block to split
    for (int i = order + 1; i <= MAX_ORDER; i++) {
        if (alloc.free_lists[i] != NULL) {
            // A larger partition found, split
            uint8_t next_order = split(i, order);

            if (next_order == MAX_ORDER + 1) return 0;

            buddy_block_t *block_size = alloc.free_lists[next_order];

            uint32_t address = (uint32_t)block_size - 0xC0000000;

            free_list_remove(address, next_order);

            // Mark block as used in the bit tree
            set_state(address, next_order, 1);

            return address;
        }
    }

    return 0;
}

void pmm_free(uint32_t address, uint32_t length) {
    uint8_t order = get_order(length);

    uint8_t state = get_state(address, order);

    if (state == 0) {
        return;
    }

    uint32_t buddy_address = ((address - alloc.base) ^ 1 << (order + MIN_BLOCK_LOG2)) + alloc.base;
    uint8_t buddy_state = get_state(buddy_address, order);

    // Buddy is either split or allocated - immediately return the block to free lists
    if (buddy_state == 1) {
        free_list_append(address, order);

        // Mark block as free in the bit tree
        set_state(address, order, 0);

        return;
    }

    // Buddy is free - merge
    while (order < MAX_ORDER) {
        free_list_remove(buddy_address, order);

        // Mark first buddy as free in the bit tree
        set_state(address, order, 0);

        order++;

        // Mark the parent block as free in the bit tree
        set_state(address, order, 0);

        // Get state of buddy of the parent block
        buddy_address = ((address - alloc.base) ^ 1 << (order + MIN_BLOCK_LOG2)) + alloc.base;
        buddy_state = get_state(buddy_address, order);

        if (buddy_state != 0) break;
    }

    // Add the final, merged block back to free lists
    free_list_append(address, order);
}