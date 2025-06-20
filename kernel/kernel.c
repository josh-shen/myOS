#include <stdio.h>

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
	
	pmm_init(multiboot_info_ptr);

	timer_init(1);
	keyboard_init();

	//printf("Hello, kernel World!\n");
	//printf("%d %d %x\n", 0, 123456789, 1234);
	//asm volatile ("int $0x3");
}
