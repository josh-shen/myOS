#include <stdint.h>
#include <string.h>

#include <idt.h>

static idt_entry_t idt_entries[256];
idt_ptr_t ip;

extern void idt_flush(uint32_t);

void init_idt() {
    ip.limit = sizeof(idt_entry_t) * 256 - 1;
    ip.base = (uint32_t)&idt_entries;
    memset(&idt_entries, 0 , sizeof(idt_entry_t) * 256);
    idt_flush((uint32_t)&ip);
}

void set_idt_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
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