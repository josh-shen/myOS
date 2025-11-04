#include <stdint.h>
#include <stdio.h>

#include <memory.h>

static uint32_t get_current_pd();
static uint32_t create_new_pt();
static void split(vm_area_t *, uint32_t);
static void merge(vm_area_t *);
static void *get_vm_area(uint32_t);

page_directory_t boot_page_directory __attribute__((section(".page_tables")))__attribute__((aligned(4096)));
// Four page tables used for kernel mapping during boot
page_table_t boot_page_tables[4] __attribute__((section(".page_tables")))__attribute__((aligned(4096)));

// Kernel vm area linked list
vm_area_t *head = NULL;

static uint32_t get_current_pd() {
    uint32_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));

    return cr3;
}

static uint32_t create_new_pt() {
    uint32_t *pt_addr = pmm_malloc(4096); // One page table fits in 4 KiB
    
    lru_cache_add(pt_addr + 0xC0000000);

    page_table_t *pt = (page_table_t *)(pt_addr + 0xC0000000);

    for (int i = 0; i < 1024; i++) {
        pt->entries[i] = 0;
    }
    
    return (uint32_t)pt_addr;
}

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

static void merge(vm_area_t *node) {
    vm_area_t *next = node->next;

    while (next != NULL) {
        if (next->used == 1) {
            node = node->next;
            return;
        }

        node->size = node->size + next->size;
        node->next = next->next;

        kfree(next, sizeof(next));
    }
}

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

void vmm_init(uint32_t virt_addr_base) {
    uint32_t length = 0xFFFFFFFF - virt_addr_base;

    // Allocate a page for inital linked list node
    uint32_t *addr = pmm_malloc(4096);

    // Create one 4 KiB node - this will be used to initialize the slab allocator
    vm_area_t *page_node = (vm_area_t *)(addr + 0xC0000000);
    page_node->addr = virt_addr_base;
    page_node->size = 4096;
    page_node->used = 0;
    page_node->next = NULL;

    head = page_node;
    
    addr += sizeof(vm_area_t);
    virt_addr_base += 4096;
    length -= 4096;

    // Create a node for the rest of the virtual memory area
    vm_area_t *node = (vm_area_t *)(addr + 0xC0000000);
    node->addr = virt_addr_base;
    node->size = length;
    node->used = 0;
    node->next = head;
    
    head->next = node;
}

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

uint32_t *vmm_malloc(uint32_t length) {
    uint32_t *virt_addr = get_vm_area(length);

    if (virt_addr == NULL) { /* TODO: handle out of virtual memory */ }

    uint32_t *curr_virt_addr = virt_addr;

    while (length >= 4096) {
        uint32_t *phys_addr = pmm_malloc(4096);

        lru_cache_add(curr_virt_addr);
        
        vmm_map((uint32_t)curr_virt_addr, (uint32_t)phys_addr, 0x3);
        
        curr_virt_addr += 4096;
        length -= 4096;
    }    
    
    return virt_addr;
}

void vmm_free(uint32_t virt_addr, uint32_t length) {
    vm_area_t *node = head;

    while (node != NULL) {
        if (node->addr == virt_addr) {
            node->used = 0;
            merge(node);
        }

        node = node->next;
    }

    while (length >= 4096) {
        uint32_t phys_addr = vmm_unmap(virt_addr);

        lru_cache_del(virt_addr);

        pmm_free(phys_addr, 4096);

        virt_addr += 4096;
        length -= 4096;
    }
}