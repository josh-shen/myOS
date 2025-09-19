#include <stdio.h>

#include <multiboot.h>
#include <memory.h>
#include <tty.h>
#include <interrupts.h>
#include <timer.h>
#include <keyboard.h>

void kernel_main(uint32_t magic, uint32_t multiboot_info_ptr) {
	terminal_init();

	gdt_init();
	idt_init();
	isr_init();
	irq_init(); // Interrupts enabled
	
	multiboot_info_t *mbi = (multiboot_info_t*)multiboot_info_ptr;
    // TODO: Pass address of command line to kernel
	
    // Check if bit 6 of flags is set for mmap_*
    if (!(mbi->flags & (1 << 6))) {
        printf("Memory map not available\n");
		return;
    } 

	// Iinitialize physical memory manager
	uint32_t virt_addr_start = pmm_init(mbi->mmap_addr, mbi->mmap_length);

	// Initialize virtual memory manager
	vmm_init(virt_addr_start);

	// Initialize kernel heap
	kmem_init();
	
	//timer_init(1);
	//keyboard_init();

	printf("Hello, kernel World!\n");
}
