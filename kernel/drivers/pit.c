#include "kernel/pit.h"
#include "kernel/io.h"

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43

/* The PIT's crystal runs at ~1.193182 MHz; you can't set a frequency
   directly, only a divisor of that base rate. */
#define PIT_BASE_FREQUENCY 1193182

void pit_init(unsigned int frequency_hz){
    unsigned int divisor = PIT_BASE_FREQUENCY / frequency_hz;

    outb(PIT_COMMAND, 0x36); /* channel 0, lobyte/hibyte access, mode 3 (square wave), binary */
    outb(PIT_CHANNEL0, (unsigned char)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (unsigned char)((divisor >> 8) & 0xFF));
}
