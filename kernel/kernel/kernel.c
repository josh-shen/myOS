#include <stdio.h>

#include <tty.h>
#include <gdt.h>
#include <idt.h>
#include <isr.h>

void kernel_main(void) {
	terminal_initialize();
	init_gdt();
	init_idt();
	init_isr();
	
	printf("Hello, kernel World!\n");
	printf("%d %d", 0, 123456789);
	__asm__ volatile ("int $0x1");
}
