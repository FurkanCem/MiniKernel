#ifndef KERNEL_TIMER_H
#define KERNEL_TIMER_H

void timer_driver_init(unsigned int frequency_hz);

unsigned long long timer_get_ticks(void);

#endif
