#ifndef _ISRS_H
#define _ISRS_H

#include <stdint.h>

struct registers {
   uint32_t ds;                                       // Processor state before interrupt
   uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;   // Pushed by pusha
   uint32_t int_num, err_code;                        // Interrupt number and error code (if applicable)
   uint32_t eip, cs, eflags;                          // Pushed by the CPU
};
typedef struct registers registers_t; 

void init_isr(void);
void isr_handler(registers_t regs);

#endif