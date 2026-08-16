#include "kernel/e820.h"
#include "kernel/fs.h"
#include "kernel/gdt.h"
#include "kernel/heap.h"
#include "kernel/idt.h"
#include "kernel/keyboard.h"
#include "kernel/klog.h"
#include "kernel/pic.h"
#include "kernel/pmm.h"
#include "kernel/shell.h"
#include "kernel/thread.h"
#include "kernel/timer.h"
#include "kernel/ufs.h"
#include "kernel/users.h"
#include "kernel/video.h"
#include "kernel/vmm.h"

void kernel_main(void) {
  clearwin();

  klog_init();
  klog_write("MiniKernel: log online\n");

  gdt_init();
  klog_write("MiniKernel: GDT + TSS installed (ring 3 available)\n");

  idt_init();
  klog_write("MiniKernel: IDT loaded\n");

  e820_init();
  klog_write("MiniKernel: memory map read, ");
  klog_write_hex(e820_entry_count());
  klog_write(" regions, ");
  klog_write_hex(e820_total_usable_bytes());
  klog_write(" usable bytes\n");

  pmm_init();
  klog_write("MiniKernel: frame allocator ready, ");
  klog_write_hex(pmm_total_frames());
  klog_write(" total frames, ");
  klog_write_hex(pmm_free_frames());
  klog_write(" free\n");

  vmm_init();
  klog_write("MiniKernel: paging extended over usable memory\n");

  heap_init();
  klog_write("MiniKernel: kernel heap ready\n");

  sched_init();
  klog_write("MiniKernel: scheduler ready\n");

  pic_remap(0x20, 0x28);
  timer_driver_init(
      100); /* 100 Hz tick instead of the PIT's default ~18.2 Hz */
  keyboard_driver_init();

  pic_unmask_irq(0); /* IRQ0: PIT timer */
  pic_unmask_irq(1); /* IRQ1: keyboard */
  __asm__ volatile(
      "sti"); /* interrupts were off since boot - turn them on now */

  klog_write("MiniKernel: PIC remapped, timer + keyboard IRQs live\n");

  fs_init();
  ufs_init();
  users_init();

  const char *welcome_msg = "Working kernel";
  putstr(welcome_msg);
  putstr_at("IDT loaded", 1);

  // __asm__ volatile ("int3");
  // volatile int z = 0; volatile int y = 1 / z;

  shell_run(); /* never returns */
}
