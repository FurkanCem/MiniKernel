#include "kernel/irq.h"
#include "kernel/pit.h"
#include "kernel/timer.h"
#include "kernel/klog.h"
#include "kernel/thread.h"

static volatile unsigned long long ticks = 0;

static void timer_irq_handler(registers_t *regs){
    (void)regs;

    ticks++;
    if (ticks % 100 == 0){
        klog_write("timer: ~1s elapsed (tick=");
        klog_write_hex(ticks);
        klog_write(")\n");
    }

    sched_yield();
}

unsigned long long timer_get_ticks(void){
    return ticks;
}

void timer_driver_init(unsigned int frequency_hz){
    pit_init(frequency_hz);
    irq_register_handler(0, timer_irq_handler);
}
