#ifndef KERNEL_PIT_H
#define KERNEL_PIT_H

/* Programs PIT channel 0 to fire IRQ0 at approximately frequency_hz. */
void pit_init(unsigned int frequency_hz);

#endif
