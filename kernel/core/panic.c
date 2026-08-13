#include "kernel/panic.h"
#include "kernel/klog.h"
#include "kernel/thread.h"
#include "kernel/process.h"
#include "kernel/video.h"
#include "kernel/vmm_stack.h"

#define CS_RPL_MASK 0x3
#define CS_RPL_USER 0x3

static void halt_forever(void) {
  for (;;) {
    __asm__ volatile("cli; hlt");
  }
}

static void log_common_regs(registers_t *regs) {
  if (regs == 0)
    return;

  klog_write("  vector:      ");
  klog_write_hex(regs->vector);
  klog_write("\n  error_code:  ");
  klog_write_hex(regs->error_code);
  klog_write("\n  faulting rip:");
  klog_write_hex(regs->rip);
  klog_write("\n  rsp:         ");
  klog_write_hex(regs->rsp);
  klog_write("\n");
}

static int fault_from_usermode(registers_t *regs) {
  if (regs == 0)
    return 0;

  return (regs->cs & CS_RPL_MASK) == CS_RPL_USER;
}

static void kill_faulting_process(const char *reason, registers_t *regs) {
  klog_write("\n*** user process crashed: ");
  klog_write(reason);
  klog_write(" (tid ");
  klog_write_hex((unsigned long long)sched_current_tid());
  klog_write(") ***\n");

  log_common_regs(regs);

  process_exit(-1);
}

void kpanic(const char *reason, registers_t *regs) {
  if (fault_from_usermode(regs)) {
    kill_faulting_process(reason, regs);
    return;
  }

  clearwin();
  putstr("*** KERNEL PANIC ***");
  putstr_at(reason, 1);

  klog_write("\n*** KERNEL PANIC: ");
  klog_write(reason);
  klog_write(" ***\n");

  log_common_regs(regs);

  halt_forever();
}

void kpanic_page_fault(unsigned long long fault_addr, registers_t *regs) {
  /*
   * Give the VM subsystem a chance to resolve
   * the fault first (e.g. grow user stack).
   */
  if (vmm_handle_page_fault(fault_addr, regs))
    return;

  if (fault_from_usermode(regs)) {
    kill_faulting_process("Page Fault", regs);
    return;
  }

  clearwin();
  putstr("*** KERNEL PANIC ***");
  putstr_at("Page Fault", 1);

  klog_write("\n*** KERNEL PANIC: Page Fault ***\n");

  klog_write("  fault address (CR2): ");
  klog_write_hex(fault_addr);
  klog_write("\n");

  if (regs != 0) {
    unsigned long long ec = regs->error_code;

    klog_write("  cause: ");
    klog_write(ec & 0x1 ? "protection violation" : "page not present");

    klog_write(", ");
    klog_write(ec & 0x2 ? "write" : "read");

    klog_write(", ");
    klog_write(ec & 0x4 ? "user mode" : "supervisor mode");

    if (ec & 0x8)
      klog_write(", reserved bit set in page table (corrupted entry)");

    if (ec & 0x10)
      klog_write(", instruction fetch");

    klog_write("\n");
  }

  log_common_regs(regs);
  halt_forever();
}
