#include <stdio.h>

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
	
	printf("Hello, kernel World!\n");
	printf("%d %d\n", 0, 123456789);
	// asm volatile ("int $0x3");
	//init_timer(1);
	init_keyboard();
	while(1){
		continue;
	}
}
