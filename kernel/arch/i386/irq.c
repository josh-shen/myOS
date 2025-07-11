#include <stdint.h>
#include <io.h>
#include <stdio.h>

#include <interrupts.h>

static void irq_remap(void);

extern void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags);
extern void irq0();
extern void irq1();
extern void irq2();
extern void irq3();
extern void irq4();
extern void irq5();
extern void irq6();
extern void irq7();
extern void irq8();
extern void irq9();
extern void irq10();
extern void irq11();
extern void irq12();
extern void irq13();
extern void irq14();
extern void irq15();

#define PIC_M 0x20
#define PIC_M_C PIC_M
#define PIC_M_D (PIC_M + 1)
#define PIC_S 0xA0
#define PIC_S_C PIC_S
#define PIC_S_D (PIC_S + 1)

static isr_t interrupt_handlers[16] = {((void *)0)};

static void irq_remap() {
    outb(PIC_M_C, 0x11);
    outb(PIC_S_C, 0x11); 
    outb(PIC_M_D, 0x20);
    outb(PIC_S_D, 0x28);
    outb(PIC_M_D, 0x04);
    outb(PIC_S_D, 0x02);
    outb(PIC_M_D, 0x01);
    outb(PIC_S_D, 0x01);
    // Unmask master and slave PICs
    outb(PIC_M_D, 0x0);
    outb(PIC_S_D, 0x0);
 }
 
void irq_init() {
    irq_remap();
    idt_set_gate(32, (uint32_t)irq0, 0x08, 0x8E);
    idt_set_gate(33, (uint32_t)irq1, 0x08, 0x8E);
    idt_set_gate(34, (uint32_t)irq2, 0x08, 0x8E);
    idt_set_gate(35, (uint32_t)irq3, 0x08, 0x8E);
    idt_set_gate(36, (uint32_t)irq4, 0x08, 0x8E);
    idt_set_gate(37, (uint32_t)irq5, 0x08, 0x8E);
    idt_set_gate(38, (uint32_t)irq6, 0x08, 0x8E);
    idt_set_gate(39, (uint32_t)irq7, 0x08, 0x8E);
    idt_set_gate(40, (uint32_t)irq8, 0x08, 0x8E);
    idt_set_gate(41, (uint32_t)irq9, 0x08, 0x8E);
    idt_set_gate(42, (uint32_t)irq10, 0x08, 0x8E);
    idt_set_gate(43, (uint32_t)irq11, 0x08, 0x8E);
    idt_set_gate(44, (uint32_t)irq12, 0x08, 0x8E);
    idt_set_gate(45, (uint32_t)irq13, 0x08, 0x8E);
    idt_set_gate(46, (uint32_t)irq14, 0x08, 0x8E);
    idt_set_gate(47, (uint32_t)irq15, 0x08, 0x8E);
    __asm__ volatile ("sti"); // Enable interrupts
}

void irq_set_handler(uint8_t n, isr_t handler) {
    interrupt_handlers[n] = handler;
}

void irq_eoi(uint8_t irq_num) {
    if (irq_num >= 8)
        outb(PIC_S_C, 0x20); 
    outb(PIC_M_C, 0x20);
}

void irq_handler(registers_t regs) {
    __asm__ volatile ("cli");
    if (regs.int_num >= 32 && regs.int_num <= 47 && interrupt_handlers[regs.int_num - 32] != ((void *)0)) {  
        isr_t handler = interrupt_handlers[regs.int_num - 32];
        handler(regs);
    } else {
        irq_eoi(regs.int_num - 32);
    }
    __asm__ volatile ("sti");
}