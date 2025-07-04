#include <stdint.h>
#include <stdio.h>

#include <interrupts.h>

static void gdt_set_gate(int32_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran);

extern void gdt_flush(uint32_t);

static gdt_entry_t gdt_entries[5];
static gdt_ptr_t gp;

void gdt_init() {
    gp.limit = (sizeof(gdt_entry_t) * 5) - 1;
    gp.base = (uint32_t)&gdt_entries;
    gdt_set_gate(0, 0, 0, 0, 0);                // null segment
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); // code segment cs
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF); // data segment ds
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF); // user mode code segment
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF); // user mode data segment
    gdt_flush((uint32_t)&gp);
}

static void gdt_set_gate(int32_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    // Encode base
    gdt_entries[num].base_low       = (base & 0xFFFF);
    gdt_entries[num].base_middle    = (base >> 16) & 0xFF;
    gdt_entries[num].base_high      = (base >> 24) & 0xFF;
    // Encode limit
    gdt_entries[num].limit_low      = (limit & 0xFFFF);
    gdt_entries[num].granularity    = (limit >> 16) & 0x0F;
    // Encode flags
    gdt_entries[num].granularity    |= gran & 0xF0;
    // Encode access
    gdt_entries[num].access         = access;
}