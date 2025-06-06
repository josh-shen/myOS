#include <stdint.h>
#include <stdio.h>
#include <io.h>

#include <timer.h>
#include <interrupts.h>

#define TIMER_IRQ 0
#define PIT_0 0x40
#define PIT_COM_PORT 0x43
uint32_t tick = 0;

void timer_callback() {
   tick++;
   printf("Tick: %d\n", tick);
   irq_eoi(TIMER_IRQ);
}

void timer_init(uint32_t frequency) {
   irq_set_handler(TIMER_IRQ, &timer_callback);
   uint32_t divisor = 1193180 / frequency;
   outb(PIT_COM_PORT, 0x36); // 00 (channel 0) 11 (low byte/high byte) 011 (square wave) 0 (16-bit binary)
   // Split divisor into upper/lower bytes
   uint8_t l = (uint8_t)(divisor & 0xFF);
   uint8_t h = (uint8_t)( (divisor >> 8) & 0xFF );
   // Send the frequency divisor
   outb(PIT_0, l);
   outb(PIT_0, h);
} 