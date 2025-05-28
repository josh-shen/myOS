#include <stdio.h>
#include <stdint.h>

#include <multiboot.h>
#include <interrupts.h>
#include <timer.h>
#include <tty.h>
#include <keyboard.h>

extern multiboot_info_t *multiboot_info_ptr;

void kernel_main(void) {
	multiboot_info_t *mbi = multiboot_info_ptr;
	
	init_gdt();
	init_idt();
	init_isr();
	init_irq(); // Interrupts enabled

	//init_timer(1);
	terminal_initialize();
	init_keyboard();

	// Memory management
	printf("Flags: 0x%x\n", mbi->flags); 
	printf("Mem lower: 0x%x\n", mbi->mem_lower);
	printf("Mem higher: 0x%x\n", mbi->mem_upper);
	
	// Check if bit 6 of flags is set
	if (mbi->flags & (1 << 6)) {
		printf("mmap length: 0x%x\n", mbi->mmap_length);
		printf("mmap addr: 0x%x\n", mbi->mmap_addr);

		// Pointer to first memory map entry
		mmap_entry_t *mmap_entry = (mmap_entry_t *)(uintptr_t)mbi->mmap_addr;

		uint64_t total_memory = 0x0;

		while((uintptr_t)mmap_entry < (uintptr_t)mbi->mmap_addr + mbi->mmap_length) {
			uint64_t base_addr = ((uint64_t)mmap_entry->base_addr_high << 32) | mmap_entry->base_addr_low;
			uint64_t length = ((uint64_t)mmap_entry->length_high << 32) | mmap_entry->length_low;

			uint32_t type = mmap_entry->type;
			
			if (type == 0x1) {
				printf("addr %x\n", base_addr);
				printf("0x%x%x bytes of usable memory at address 0x%x%x \n", mmap_entry->length_high, mmap_entry->length_low, mmap_entry->base_addr_high, mmap_entry->base_addr_low);
			} else if (type == 0x2) {
				printf("reserved\n");
			} else if (type == 0x3) {
				printf("0x%x%x bytes of ACPI reclaimable memory at address 0x%x%x \n", mmap_entry->length_high, mmap_entry->length_low, mmap_entry->base_addr_high, mmap_entry->base_addr_low);
			} else if (type == 0x4) {
				printf("Reserved\n");
			} else if (type == 0x5) {
				printf("defective\n");
			} else {
				printf("other\n"); // There should never be "other" types
			}			
			printf("\n");
			
			// Add memory map entry size + size field
			mmap_entry = (mmap_entry_t *)((uintptr_t)mmap_entry + mmap_entry->size + sizeof(mmap_entry->size));
		}
	} else {
		printf("Memory map not available\n");
		
	}
	
	//printf("Hello, kernel World!\n");
	//printf("%d %d %x\n", 0, 123456789, 1234);
	// asm volatile ("int $0x3");
}
