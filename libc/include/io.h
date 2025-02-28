#ifndef _IO_H
#define _IO_H 1

#include <sys/cdefs.h>

#include <stdint.h>

#define EOF (-1)

#ifdef __cplusplus
extern "C" {
#endif

void outb(uint16_t port, uint8_t val);
uint8_t inb(uint16_t port);

#ifdef __cplusplus
}
#endif

#endif