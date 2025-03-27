#ifndef _INTERRUPTS_H
#define _INTERRUPTS_H

#include <stdint.h>

/* Global Descriptor Table */
struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} __attribute__((packed));
typedef struct gdt_entry gdt_entry_t;

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));
typedef struct gdt_ptr gdt_ptr_t;

void init_gdt(void);

/* Interrupt Descriptor Table */
struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t zero;
    uint8_t attributes;
    uint16_t offset_high;
} __attribute__((packed));
typedef struct idt_entry idt_entry_t;

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));
typedef struct idt_ptr idt_ptr_t;

void init_idt(void);
void set_idt_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags);

/* Interrupt Service Routines */
struct registers {
   uint32_t esp_dump;                                 // Function argument pointer
   uint32_t ds;                                       // Processor state before interrupt
   uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;   // Pushed by pusha
   uint32_t int_num, err_code;                        // Interrupt number and error code (if applicable)
   uint32_t eip, cs, eflags;                          // Pushed by the CPU
};
typedef struct registers registers_t; 

void init_isr(void);
void isr_handler(registers_t regs);

/* Interrupt Requests */
typedef void (*isr_t)();

void init_irq(void);
void set_irq_handler(uint8_t n, isr_t handler);
void irq_handler(registers_t regs);
void irq_eoi(uint8_t irq_num);

#endif