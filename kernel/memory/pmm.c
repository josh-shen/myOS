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

buddy_t pmm __attribute__((section(".buddy_allocator")));

/**
 * @brief Rounds an integer up to the nearest power of 2.
 *
 * This function uses bit manipulation to compute the smallest power of 2 that
 * is greater than or equal to @p length.
 *
 * @param length The integer value to be rounded up.
 * @return The smallest power of 2 greater than or equal to length.
 */
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

/**
 * @brief Calculates the order of a memory block for the buddy allocator.
 *
 * This function determines the order of a memory block based on its length. It
 * searches from the maximum order down to find the largest order that fits in
 * @p length.
 *
 * @param length The size of the memory block (in bytes).
 * @return The order value (0 to MAX_ORDER) corresponding to the block size.
 */
static uint8_t get_order(uint32_t length) {
    for (int n = MAX_BLOCK_LOG2; n >= MIN_BLOCK_LOG2; n--) {
        if ((unsigned)(1 << n) <= length) return n - MIN_BLOCK_LOG2;
    }
    return 0;
}

/**
 * @brief Calculates the bit tree index for a memory block.
 *
 * This function finds the position of a memory block in the buddy allocator's
 * bit tree structure. The index is calculated based on the block's address and
 * order, accounting for truncated tree nodes at higher levels.
 *
 * @param address The physical address of the memory block.
 * @param order The order of the memory block.
 * @return The index of the block in the bit tree array.
 */
static uint32_t get_bit_tree_index(uint32_t address, uint8_t order) {
    uint8_t height = MEM_BLOCK_LOG2 - order - MIN_BLOCK_LOG2;
    uint32_t offset = (address - pmm.base) / (1 << (MIN_BLOCK_LOG2 + order));
    uint32_t node_index = (1 << height) - 1 + offset - TRUNCATED_TREE_NODES;

    return node_index;
}

/**
 * @brief Retrieves the allocation state of a memory block from the bit tree.
 *
 * This function reads the state bit for a specific memory block from the buddy
 * allocator's bit tree.
 *
 * @param address The physical address of the memory block.
 * @param order The order of the memory block.
 * @return The state bit value (0 for free, 1 for allocated/split).
 */
static uint8_t get_state(uint32_t address, uint8_t order) {
    uint32_t index = get_bit_tree_index(address, order);
    uint32_t word_index = index / 32;
    uint32_t word_offset = index % 32;

    uint8_t state = pmm.bit_tree[word_index] >> word_offset;

    // Apply mask to get the desired bit
    return state & 1;
}

/**
 * @brief Sets the state of a memory block in the bit tree.
 *
 * This function updates the state bit for a specific memory block in the buddy
 * allocator's bit tree. The function preserves all other bits in the word by
 * using a mask operation, then sets the target bit to @p state
 *
 * @param address The physical address of the memory block.
 * @param order The order of the memory block.
 * @param state The state value to set (0 for free, 1 for allocated/split).
 */
static void set_state(uint32_t address, uint8_t order, uint8_t state) {
    uint32_t index = get_bit_tree_index(address, order);
    uint32_t word_index = index / 32;
    uint32_t word_offset = index % 32;

    // Create a mask to only set the bit at the target position
    uint32_t mask = ~(1 << word_offset);

    pmm.bit_tree[word_index] = (pmm.bit_tree[word_index] & mask) | state << word_offset;
}

/**
 * @brief Adds a memory block to the free list of its order.
 *
 * This function inserts a memory block at the head of the free list
 * corresponding to its order. The block is accessed through its higher half
 * virtual address.
 *
 * @param address The physical address of the memory block to add.
 * @param order The order of the memory block.
 */
static void free_list_append(uint32_t address, uint8_t order) {
    buddy_block_t *block = (buddy_block_t *)(address + 0xC0000000);

    if (pmm.free_lists[order]) pmm.free_lists[order]->prev = block;

    block->prev = NULL;
    block->next = pmm.free_lists[order];

    pmm.free_lists[order] = block;
}

/**
 * @brief Removes a memory block from the free list of its order.
 *
 * This function extracts a memory block from its free list by updating the next
 * and previous pointers of adjacent blocks.
 *
 * @param address The physical address of the memory block to remove.
 * @param order The order of the memory block.
 */
static void free_list_remove(uint32_t address, uint8_t order) {
    buddy_block_t *block = (buddy_block_t *)(address + 0xC0000000);

    if (block->prev != NULL) block->prev->next = block->next;

    if (pmm.free_lists[order] == block) pmm.free_lists[order] = block->next;

    if (block->next != NULL) block->next->prev = block->prev;

    block->prev = NULL;
    block->next = NULL;
}

/**
 * @brief Splits a memory block down to the target order.
 *
 * This function recursively divides a memory block into smaller buddy blocks
 * until the target order is reached. At each split, the parent block is
 * removed from its free list, marked as split in the bit tree, and both
 * resulting buddy blocks are added to the free list of the next lower order.
 * The buddy address is calculated using XOR operations.
 *
 * @param order The current order of the block to split.
 * @param target The desired order after splitting.
 * @return The final order after splitting, or MAX_ORDER + 1 on error.
 */
static uint8_t split(uint8_t order, uint8_t target) {
    while (order > target) {
        buddy_block_t *block = pmm.free_lists[order];

        uint32_t address = (uint32_t)block - 0xC0000000;
        uint32_t buddy_address = ((address - pmm.base) ^ 1 << (order - 1 + MIN_BLOCK_LOG2)) + pmm.base;

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

/**
 * @brief Marks a region of memory as free and adds it to the allocator.
 *
 * This function processes a memory region by filtering out known used regions,
 * creating higher-half linear mappings, and adding free blocks to the buddy
 * allocator's free lists. Free memory regions are divided into order-sized
 * blocks, and each block is marked as 1 (free) in the bit tree.
 *
 * @param base The starting physical address of the memory region.
 * @param length The size of the memory region (in bytes).
 */
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

        pmm.size += block_size;
        
        free_list_append(base, order);

        set_state(base, order, 0);

        base += block_size;
        length -= block_size;
    }
}

/**
 * @brief Initializes the physical memory manager and buddy allocator.
 *
 * This function sets up the physical memory manager using the multiboot memory
 * map to discover usable memory regions. It initializes the bit tree to all 1
 * (allocated), initializes the free lists, and processes memory map entries to
 * mark available regions as free.
 *
 * @param mmap_addr The physical address of the multiboot memory map.
 * @param mmap_length The length of the memory map (in bytes).
 * @return The last virtual address used for linear mapping plus a one-page gap.
 */
uint32_t pmm_init(uint32_t mmap_addr, uint32_t mmap_length) {
    pmm.base = 0; 
    pmm.size = 0;

    // Ensure regions are a power of 2, if not round it up
    for (int i = 0; i < NUM_USED_REGIONS; i++) {
        if ((used_regions[i][1] & (used_regions[i][1] - 1)) != 0) {
            used_regions[i][1] = round_pow2(used_regions[i][1]);
        }
    }

    // Initialize bit tree - all bits initially set to 1, only free blocks will be changed to 0
    for (int i = 0; i < TREE_WORDS; i++) {
        pmm.bit_tree[i] = 0xFFFFFFFF;
    }

    // Initialize free lists
    for (int i = 0; i <= MAX_ORDER; i++) {
        pmm.free_lists[i] = NULL;
    }

    // Pointer to first memory map entry
    mmap_entry_t *mmap_entry = (mmap_entry_t *)mmap_addr;

    uint32_t addr_end = 0;

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
    
    // Return last virtual address used for linear mappings with a one-page gap
    return addr_end + 0xC0001000;
}

/**
 * @brief Allocates a physical memory block of the requested size.
 *
 * This function finds and allocates a memory block from the buddy allocator
 * that satisfies the requested size. It first attempts to allocate a block of
 * the exact order requested. If unavailable, it searches for a larger block and
 * splits it to the required size. The allocated block is removed from its free
 * list and marked as used in the bit tree.
 *
 * @param length The size of memory to allocate (in bytes).
 * @return Pointer to the physical address of the allocated block, or NULL if
 *         allocation fails or length exceeds maximum block size.
 */
uint32_t *pmm_malloc(uint32_t length) {
    if (length > 1 << MAX_BLOCK_LOG2) return NULL;

    uint8_t order = get_order(length);

    /*
    TODO: wake kswapd if free pages falls below low_watermark
    uint32_t free_pages = (pmm.size - (1 << order)) / 4096;

    if (free_pages < low_watermark) wake(kswapd)

    block until kswapd is done?
    */

    // Block of the requested order is available - no need to split
    if (pmm.free_lists[order] != NULL) {
        buddy_block_t *block = pmm.free_lists[order];

        uint32_t address = (uint32_t)block - 0xC0000000;

        free_list_remove(address, order);

        // Mark block as used in the bit tree
        set_state(address, order, 1);

        pmm.size -= 1 << (order + MIN_BLOCK_LOG2);

        return (uint32_t *)address;
    }

    // A best fit block was not available - search for a larger block to split
    for (int i = order + 1; i <= MAX_ORDER; i++) {
        if (pmm.free_lists[i] != NULL) {
            // A larger partition found, split
            uint8_t next_order = split(i, order);

            if (next_order == MAX_ORDER + 1) return NULL;

            buddy_block_t *block = pmm.free_lists[next_order];

            uint32_t address = (uint32_t)block - 0xC0000000;

            free_list_remove(address, next_order);

            // Mark block as used in the bit tree
            set_state(address, next_order, 1);

            pmm.size -= 1 << (next_order + MIN_BLOCK_LOG2);

            return (uint32_t *)address;
        }
    }

    return NULL;
}

/**
 * @brief Frees a previously allocated physical memory block.
 *
 * This function returns a memory block to the buddy allocator and attempts to
 * merge it with its buddy if the buddy is also free. Merging continues while
 * both buddies remain free. The final merged block is added to the appropriate
 * free list and marked as free in the bit tree.
 *
 * @param address The physical address of the memory block to free.
 * @param length The size of the memory block (in bytes).
 */
void pmm_free(uint32_t address, uint32_t length) {
    uint8_t order = get_order(length);

    uint8_t state = get_state(address, order);

    if (state == 0) return; // TODO: handle error if the state is 0. This probably means the length is not correct

    uint32_t buddy_address = ((address - pmm.base) ^ 1 << (order + MIN_BLOCK_LOG2)) + pmm.base;
    uint8_t buddy_state = get_state(buddy_address, order);

    // Buddy is either split or allocated - immediately return the block to free lists
    if (buddy_state == 1) {
        free_list_append(address, order);

        // Mark block as free in the bit tree
        set_state(address, order, 0);

        pmm.size += 1 << (order + MIN_BLOCK_LOG2);

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
        buddy_address = ((address - pmm.base) ^ 1 << (order + MIN_BLOCK_LOG2)) + pmm.base;
        buddy_state = get_state(buddy_address, order);

        if (buddy_state != 0) break;
    }

    // Add the final, merged block back to free lists
    free_list_append(address, order);

    pmm.size += 1 << (order + MIN_BLOCK_LOG2);
}