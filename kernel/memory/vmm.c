#include <stdint.h>
#include <stddef.h>

#include <memory.h>

static uint32_t get_current_pd();
static uint32_t create_new_pt();
static void split(vm_area_t *, uint32_t);
static void merge(vm_area_t *);
static void *get_vm_area(uint32_t);

page_directory_t boot_page_directory __attribute__((section(".page_tables")))__attribute__((aligned(PAGE_SIZE)));
// Four page tables used for kernel mapping during boot
page_table_t boot_page_tables[4] __attribute__((section(".page_tables")))__attribute__((aligned(PAGE_SIZE)));

// Kernel vm area linked list
vm_area_t *head = NULL;

/**
 * @brief Retrieves the current page directory address from the CR3 register.
 *
 * @return The physical address of the current page directory.
 */
static uint32_t get_current_pd() {
    uint32_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));

    return cr3;
}

/**
 * @brief Creates and initializes a new page table.
 *
 * This function allocates a 4 KiB page from physical memory to hold a new page
 * table. All 1024 entries are initialized to zero, marking them as not present.
 *
 * @return The physical address of the newly created page table.
 */
static uint32_t create_new_pt() {
    uint32_t *pt_addr = pmm_malloc(PAGE_SIZE); // One page table fits in 4 KiB

    page_table_t *pt = (page_table_t *)(pt_addr + 0xC0000000);

    for (int i = 0; i < 1024; i++) {
        pt->entries[i] = 0;
    }
    
    return (uint32_t)pt_addr;
}

/**
 * @brief Splits a virtual memory area node into two separate nodes.
 *
 * This function divides a virtual memory area node at @p length. The original
 * node is resized to @p length, and a new node is allocated with kmalloc and
 * sized to the remainder of the original size. The new node is inserted into
 * the linked list of virutal memory areas immediately after the original node.
 *
 * @param node Pointer to the vm_area_t node to be split.
 * @param length The size of the node to be split off (in bytes).
 */
static void split(vm_area_t *node, uint32_t length) {
    uint32_t *addr = kmalloc(sizeof(vm_area_t));

    // Create new node of size (original length - length)
    vm_area_t *split = (vm_area_t *)addr;
    split->addr = node->addr + length;
    split->size = node->size - length;
    split->used = 0;
    split->next = node->next;

    node->size = length;
    node->next = split;
}

/**
 * @brief Merges free virtual memory area nodes.
 *
 * This function attempts to merge @p node with all consecutive unused nodes in
 * the linked list of virtual memory areas. Merging continues until a used node
 * is encountered or the end of the list is reached. Memory usesd by leftover
 * merged nodes is freed using kfree.
 *
 * @param node Pointer to the starting vm_area_t node for merging.
 */
static void merge(vm_area_t *node) {
    vm_area_t *next = node->next;

    while (next != NULL) {
        if (next->used == 1) {
            return;
        }

        node->size = node->size + next->size;
        node->next = next->next;

        kfree(next, sizeof(vm_area_t));
    }
}

/**
 * @brief Finds and allocates a virtual memory area of the requested size.
 *
 * This function searches the linked list of virtual memory areas for an unused
 * region that can accommodate the requested length. If a larger region is
 * found, it is split to match the exact size needed. The found region is
 * marked as used and its address is returned.
 *
 * @param length The size of the virtual memory area needed (in bytes).
 * @return Pointer to the starting address of the allocated virtual memory area,
 *         or NULL if no suitable area is found.
 */
static void *get_vm_area(uint32_t length) {
    vm_area_t *node = head;

    while (node != NULL) {
        if (node->used == 1 || node->size < length) {
            node = node->next;
            continue;
        }

        // Split if a larger than needed node is found
        if (node->size > length) split(node, length);
        
        node->used = 1;

        return (uint32_t *)node->addr;
    }
    
    return NULL;
}

/**
 * @brief Initializes the virtual memory manager.
 *
 * This function sets up the virtual memory managemer by creating the initial
 * linked list of vm_area_t nodes. It reserves one 4 KiB node for slab allocator
 * initialization and creates a second node for the remaining virtual address
 * space from @p virt_addr_base to 0xFFFFFFFF. Both nodes are initially marked
 * as unused.
 *
 * @param virt_addr_base The starting virtual address for the managed memory
 *        region.
 */
void vmm_init(uint32_t virt_addr_base) {
    uint32_t length = 0xFFFFFFFF - virt_addr_base;

    // Allocate a page for inital linked list node
    uint32_t addr = (uint32_t)pmm_malloc(PAGE_SIZE);

    // Create one 4 KiB node - this will be used to initialize the slab allocator
    vm_area_t *page_node = (vm_area_t *)(addr + 0xC0000000);
    page_node->addr = virt_addr_base;
    page_node->size = PAGE_SIZE;
    page_node->used = 0;
    page_node->next = NULL;

    head = page_node;
    
    addr += sizeof(vm_area_t);
    virt_addr_base += PAGE_SIZE;
    length -= PAGE_SIZE;

    // Create a node for the rest of the virtual memory area
    vm_area_t *node = (vm_area_t *)(addr + 0xC0000000);
    node->addr = virt_addr_base;
    node->size = length;
    node->used = 0;
    node->next = head;
    
    head->next = node;
}

/**
 * @brief Maps a virtual address to a physical address in the page tables.
 *
 * This function establishes a mapping between @p virt_addr and @p phys_addr by
 * updating the appropriate page directory and page table entries. If the
 * required page table does not exist, it is created automatically. The function
 * does not overwrite existing mappings if the page is already present.
 *
 * @param virt_addr The virtual address to be mapped.
 * @param phys_addr The physical address to map to.
 * @param flags Page table flags.
 */
void vmm_map(uint32_t virt_addr, uint32_t phys_addr, uint32_t flags) {
    uint32_t pde_index = (virt_addr >> 22) & 0x3FF;
    uint32_t pte_index = (virt_addr >> 12) & 0x3FF;

    page_directory_t *pd = (page_directory_t *)get_current_pd();

    // Check if page table exists
    if (!(pd->entries[pde_index] & PDE_PRESENT)) {
        pd->entries[pde_index] = create_new_pt() | flags;
    }
    
    // Get the physical address from the (possibly updated) PDE
    uint32_t pt_phys_addr = pd->entries[pde_index] & 0xFFFFF000;
    page_table_t *pt = (page_table_t *)(pt_phys_addr + 0xC0000000);
    
    // Check if address is already mapped
    if (!(pt->entries[pte_index] & PTE_PRESENT)) {
        pt->entries[pte_index] = phys_addr | flags;
    }
}

/**
 * @brief Unmaps a virtual address and returns its physical address.
 *
 * This function removes the mapping for @p virt_addr by clearing the
 * corresponding page table entry. The physical address mapped to @p virt_addr
 * is extracted and returned before the entry is cleared.
 *
 * @param virt_addr The virtual address to be unmapped.
 * @return The physical address that was previously mapped to the virtual
 *         address.
 */
uint32_t vmm_unmap(uint32_t virt_addr) {
    uint32_t pde_index = (virt_addr >> 22) & 0x3FF;
    uint32_t pte_index = (virt_addr >> 12) & 0x3FF;

    page_directory_t *pd = (page_directory_t *)get_current_pd();
    uint32_t pde = pd->entries[pde_index];

    uint32_t pt_phys_addr = pde & PDE_FRAME;
    page_table_t *pt = (page_table_t *)(pt_phys_addr + 0xC0000000);

    uint32_t phys_addr = pt->entries[pte_index] & PTE_FRAME;
    pt->entries[pte_index] = 0;

    return phys_addr;
}

/**
 * @brief Allocates virtual memory with physical page backing.
 *
 * This function allocates a contiguous virtual memory region of @p length and
 * maps it to physical memory pages. Physical pages are allocated in 4 KiB
 * chunks and mapped to consecutive virtual addresses with read/write
 * permissions (flags 0x3). The allocation is performed page by page until the
 * requested length is satisfied.
 *
 * @param length The size of the memory region to allocate (in bytes).
 * @return Pointer to the starting virtual address of the allocated memory, or
 *         NULL if virtual memory allocation fails.
 */
uint32_t *vmm_malloc(uint32_t length) {
    uint32_t *virt_addr = get_vm_area(length);

    if (virt_addr == NULL) return NULL; // TODO: implement better error handlng. page fault or call kswapd and retry?

    uint32_t *curr_virt_addr = virt_addr;

    while (length >= PAGE_SIZE) {
        uint32_t *phys_addr = pmm_malloc(PAGE_SIZE);
        
        vmm_map((uint32_t)curr_virt_addr, (uint32_t)phys_addr, 0x3);
        
        curr_virt_addr += PAGE_SIZE;
        length -= PAGE_SIZE;
    }    
    
    return virt_addr;
}

/**
 * @brief Frees previously allocated virtual memory and its physical backing.
 *
 * This function frees a virtual memory region by marking the corresponding
 * vm_area_t node as unused and merging it with adjacent free nodes. Each page
 * is unmapped and the associated physical memory freed. Like the allocation
 * process, deallocation is performed in 4 KiB page increments.
 *
 * @param virt_addr The starting virtual address of the memory to free.
 * @param length The size of the memory region to free (in bytes).
 */
void vmm_free(uint32_t virt_addr, uint32_t length) {
    vm_area_t *node = head;

    while (node != NULL) {
        if (node->addr == virt_addr) {
            node->used = 0;
            merge(node);
        }

        node = node->next;
    }

    while (length >= PAGE_SIZE) {
        uint32_t phys_addr = vmm_unmap(virt_addr);

        pmm_free(phys_addr, PAGE_SIZE);

        virt_addr += PAGE_SIZE;
        length -= PAGE_SIZE;
    }
}