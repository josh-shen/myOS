#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

void timer_callback(void);
void timer_init(uint32_t frequency);

#endif