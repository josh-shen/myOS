#include <stdio.h>

#include <memory.h>
#include <tty.h>
#include <interrupts.h>
#include <timer.h>
#include <keyboard.h>

void kernel_main(void) {
	terminal_initialize();
		
	init_gdt();
	init_idt();
	init_isr();
	init_irq(); // Interrupts enabled

	init_memory();
	
	//init_timer(1);
	//init_keyboard();

	//printf("Hello, kernel World!\n");
	//printf("%d %d %x\n", 0, 123456789, 1234);
	// asm volatile ("int $0x3");
}
