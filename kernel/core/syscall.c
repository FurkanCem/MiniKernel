#include "kernel/syscall.h"
#include "kernel/klog.h"
#include "kernel/panic.h"
#include "kernel/thread.h"

void syscall_handler(registers_t *regs) {
  switch (regs->rax) {
  case SYS_WRITE_HELLO:
    klog_write("syscall: hello from ring 3 (tid ");
    klog_write_hex((unsigned long long)sched_current_tid());
    klog_write(")\n");
    regs->rax = 0;
    break;

  case SYS_EXIT:
    klog_write("syscall: ring-3 thread exiting via SYS_EXIT\n");
    sched_exit(); /* doesn't return - switches straight to another thread,
                   * same as a preemption from the timer IRQ */
    break;

  default:
    klog_write("syscall: unknown number ");
    klog_write_hex(regs->rax);
    klog_write(" from ring 3\n");
    regs->rax = (unsigned long long)-1;
    break;
  }
}
