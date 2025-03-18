#include <stdint.h>
#include <stdio.h>

#include <interrupts.h>

static void set_gdt_gate(int32_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran);

static gdt_entry_t gdt_entries[5];
gdt_ptr_t gp;

extern void gdt_flush(uint32_t);

void init_gdt() {
    gp.limit = (sizeof(gdt_entry_t) * 5) - 1;
    gp.base = (uint32_t)&gdt_entries;
    set_gdt_gate(0, 0, 0, 0, 0);                // null segment
    set_gdt_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); // code segment cs
    set_gdt_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF); // data segment ds
    set_gdt_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF); // user mode code segment
    set_gdt_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF); // user mode data segment
    gdt_flush((uint32_t)&gp);
}

static void set_gdt_gate(int32_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
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