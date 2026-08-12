#include "kernel/syscall.h"
#include "kernel/fd.h"
#include "kernel/klog.h"
#include "kernel/process.h"
#include "kernel/thread.h"
#include "kernel/vmm.h"

#define SYS_IO_MAX_LEN 4096ULL
#define SYS_SPAWN_CMDLINE_MAX 128ULL
#define SYS_SPAWN_NAME_MAX 20ULL
#define SYS_OPEN_NAME_MAX 32ULL

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
    int fd = (int)regs->rdi;
    unsigned long long buf = regs->rsi;
    unsigned long long len = regs->rdx;

    if (len > SYS_IO_MAX_LEN || !user_range_ok(buf, len)) {
      regs->rax = (unsigned long long)-1;
      break;
    }

    regs->rax = (unsigned long long)fd_write(fd, (const void *)buf, len);
    break;
  }

  case SYS_READ: {
    int fd = (int)regs->rdi;
    unsigned long long buf = regs->rsi;
    unsigned long long len = regs->rdx;

    if (len > SYS_IO_MAX_LEN || !user_range_ok(buf, len)) {
      regs->rax = (unsigned long long)-1;
      break;
    }

    regs->rax = (unsigned long long)fd_read(fd, (void *)buf, len);
    break;
  }

  case SYS_OPEN: {
    unsigned long long name_ptr = regs->rsi;
    unsigned long long flags = regs->rdx;

    if (!user_range_ok(name_ptr, SYS_OPEN_NAME_MAX)) {
      regs->rax = (unsigned long long)-1;
      break;
    }

    char name[SYS_OPEN_NAME_MAX];
    const char *src = (const char *)name_ptr;
    unsigned long long i = 0;
    for (; i < SYS_OPEN_NAME_MAX - 1 && src[i] != '\0'; i++) {
      name[i] = src[i];
    }
    name[i] = '\0';

    regs->rax = (unsigned long long)fd_open(name, flags);
    break;
  }

  case SYS_CLOSE: {
    int fd = (int)regs->rdi;
    regs->rax = (unsigned long long)fd_close(fd);
    break;
  }

  case SYS_SPAWN: {
    unsigned long long buf = regs->rsi;
    unsigned long long len = regs->rdx;

    if (len == 0 || len >= SYS_SPAWN_CMDLINE_MAX ||
        !user_range_ok(buf, len)) {
      regs->rax = (unsigned long long)-1;
      break;
    }

    char cmdline[SYS_SPAWN_CMDLINE_MAX];
    const char *src = (const char *)buf;
    for (unsigned long long i = 0; i < len; i++) {
      cmdline[i] = src[i];
    }

    unsigned long long split = 0;
    while (split < len && cmdline[split] != ' ')
      split++;

    if (split == 0 || split >= SYS_SPAWN_NAME_MAX) {
      regs->rax = (unsigned long long)-1;
      break;
    }

    char name[SYS_SPAWN_NAME_MAX];
    for (unsigned long long i = 0; i < split; i++) {
      name[i] = cmdline[i];
    }
    name[split] = '\0';

    unsigned long long arg_start = split;
    while (arg_start < len && cmdline[arg_start] == ' ')
      arg_start++;

    regs->rax = (unsigned long long)process_spawn_from_file(
        name, cmdline + arg_start, len - arg_start);
    break;
  }

  case SYS_WAIT: {
    int tid = (int)regs->rdi;
    sched_wait(tid);
    regs->rax = 0;
    break;
  }

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
