#include "kernel/video.h"
#include "kernel/klog.h"
#include "kernel/idt.h"
#include "kernel/pic.h"
#include "kernel/timer.h"
#include "kernel/keyboard.h"
#include "kernel/e820.h"
#include "kernel/shell.h"

void kernel_main(void){
    clearwin();

    klog_init();
    klog_write("MiniKernel: log online\n");

    idt_init();
    klog_write("MiniKernel: IDT loaded\n");

    e820_init();
    klog_write("MiniKernel: memory map read, ");
    klog_write_hex(e820_entry_count());
    klog_write(" regions, ");
    klog_write_hex(e820_total_usable_bytes());
    klog_write(" usable bytes\n");

    /* Order matters here: the PIC must be remapped (moved off vectors
       0-31) and every driver must have registered its handler before
       we unmask its IRQ or flip interrupts on globally - otherwise a
       tick could fire into a line nothing is listening on yet. */
    pic_remap(0x20, 0x28);
    timer_driver_init(100);     /* 100 Hz tick instead of the PIT's default ~18.2 Hz */
    keyboard_driver_init();

    pic_unmask_irq(0);          /* IRQ0: PIT timer */
    pic_unmask_irq(1);          /* IRQ1: keyboard */
    __asm__ volatile ("sti");   /* interrupts were off since boot - turn them on now */

    klog_write("MiniKernel: PIC remapped, timer + keyboard IRQs live\n");

    const char *welcome_msg = "Working kernel, loving nobody";
    putstr(welcome_msg);
    putstr_at("IDT loaded", 1);

    /* Sanity checks - uncomment one at a time to confirm the IDT is
       actually wired up. Both should clear the screen and print the
       exception name via kpanic() instead of silently rebooting. */
    // __asm__ volatile ("int3");
    // volatile int z = 0; volatile int y = 1 / z;

    shell_run(); /* never returns */
}
