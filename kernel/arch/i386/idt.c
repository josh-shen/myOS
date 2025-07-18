#include <stdint.h>
#include <string.h>

#include <interrupts.h>

extern void idt_flush(uint32_t);

static idt_entry_t idt_entries[256];
static idt_ptr_t ip;

void idt_init() {
    ip.limit = sizeof(idt_entry_t) * 256 - 1;
    ip.base = (uint32_t)&idt_entries;
    memset(&idt_entries, 0 , sizeof(idt_entry_t) * 256);
    idt_flush((uint32_t)&ip);
}

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    // Encode offset
    idt_entries[num].offset_low     = base & 0xFFFF;
    idt_entries[num].offset_high    = (base >> 16) & 0xFFFF;
    // Encode selector
    idt_entries[num].selector       = sel;
    // Encode zero
    idt_entries[num].zero           = 0;
    // Encode gate type, dpl, and p
    idt_entries[num].attributes     = flags;
}