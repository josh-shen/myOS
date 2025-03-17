#include <stdint.h>
#include <stdio.h>
#include <io.h>

#include <timer.h>
#include <interrupts.h>

#define TIMER_IRQ 0
uint32_t tick = 0;

static void timer_callback(registers_t regs) {
   tick++;
   printf("Tick: %d\n", tick);
   irq_eoi(TIMER_IRQ);
}

void init_timer(uint32_t frequency) {
   set_irq_handler(TIMER_IRQ, &timer_callback);
   outb(0x21, inb(0x21) & ~0x01);
   uint32_t divisor = 1193180 / frequency;
   // Send the command byte
   outb(0x43, 0x36);
   // Split divisor into upper/lower bytes
   uint8_t l = (uint8_t)(divisor & 0xFF);
   uint8_t h = (uint8_t)( (divisor >> 8) & 0xFF );
   // Send the frequency divisor
   outb(0x40, l);
   outb(0x40, h);
} 