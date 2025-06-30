#include <stdint.h>

#include <memory.h>

static int next_free_boot_pt_index = 3;

// 32 page tables required to map 128 MiB of memory
uint32_t boot_page_directory[1024]__attribute__((section(".page_tables")))__attribute__((aligned(4096)));
uint32_t boot_page_tables[32][1024]__attribute__((section(".page_tables")))__attribute((aligned(4096)));

void vmm_init_kernel_space(uint32_t base, uint32_t len) {
    while (len >= 4096) {
        uint32_t virtual_address = base + 0xC0000000;

        uint32_t pde_index = (virtual_address >> 22) & 0x3FF;
        uint32_t pte_index = (virtual_address >> 12) & 0x3FF;
        uint32_t relative_pte = pde_index - 768;

        uint32_t pde = boot_page_directory[pde_index];
    
        if (!(pde & 0x1)) {
            if (next_free_boot_pt_index++ > 31) return;
            // TODO: implement actual error handling for running out of space      

            uint32_t *new_pt_virtual = boot_page_tables[next_free_boot_pt_index];

            for (int i = 0; i < 1024; i++) {
                new_pt_virtual[i] = 0;
            }

            uint32_t new_pt_phys_addr = (uint32_t)new_pt_virtual - 0xC0000000;
      
            boot_page_directory[pde_index] = new_pt_phys_addr + 0x003;
        }

        uint32_t *target_pt_array = boot_page_tables[relative_pte];
        target_pt_array[pte_index] = base + 0x003;

        base += 4096;
        len -= 4096;
    }
}