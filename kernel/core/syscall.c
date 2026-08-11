#include "kernel/syscall.h"
#include "kernel/klog.h"
#include "kernel/thread.h"
#include "kernel/video.h"
#include "kernel/vmm.h"

#define SYS_WRITE_MAX_LEN 4096ULL

static int user_range_ok(unsigned long long vaddr, unsigned long long len) {
  if (len == 0)
    return 1;

  vmm_address_space_t space = vmm_current_address_space();
  unsigned long long start = vaddr & ~0xFFFULL;
  unsigned long long end = (vaddr + len + 0xFFF) & ~0xFFFULL;

  for (unsigned long long page = start; page < end; page += 0x1000) {
    if (!vmm_is_user_accessible_in(space, page))
      return 0;
  }
  return 1;
}

void syscall_handler(registers_t *regs) {
  switch (regs->rax) {
  case SYS_WRITE_HELLO:
    klog_write("syscall: hello from ring 3 (tid ");
    klog_write_hex((unsigned long long)sched_current_tid());
    klog_write(")\n");
    regs->rax = 0;
    break;

  case SYS_WRITE: {
    unsigned long long buf = regs->rsi;
    unsigned long long len = regs->rdx;

    if (len > SYS_WRITE_MAX_LEN || !user_range_ok(buf, len)) {
      regs->rax = (unsigned long long)-1;
      break;
    }

    klog_write_n((const char *)buf, len);
    for (unsigned long long i = 0; i < len; i++) {
      putchar_at_cursor(((const char *)buf)[i]);
    }
    regs->rax = len;
    break;
  }

  case SYS_READ:
    regs->rax = 0;
    break;

  case SYS_EXIT:
    klog_write("syscall: ring-3 thread exiting via SYS_EXIT\n");
    sched_exit();
    break;

  default:
    klog_write("syscall: unknown number ");
    klog_write_hex(regs->rax);
    klog_write(" from ring 3\n");
    regs->rax = (unsigned long long)-1;
    break;
  }
}
