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

	printf("Flags: 0x%x\n", mbi->flags); 
	printf("Mem lower: 0x%x\n", mbi->mem_lower);
	printf("Mem higher: 0x%x\n", mbi->mem_upper);
	
	printf("mmap length: 0x%x\n", mbi->mmap_length);
	printf("mmap addr: 0x%x\n", mbi->mmap_addr);

	// Pointer to first memory map entry
	mmap_entry_t *mmap_entry = (mmap_entry_t *)(uintptr_t)mbi->mmap_addr;
	
	//printf("Hello, kernel World!\n");
	//printf("%d %d %x\n", 0, 123456789, 1234);
	// asm volatile ("int $0x3");
}
