#include "kernel/syscall.h"
#include "kernel/fd.h"
#include "kernel/fs.h"
#include "kernel/klog.h"
#include "kernel/memfs.h"
#include "kernel/process.h"
#include "kernel/pmm.h"
#include "kernel/thread.h"
#include "kernel/ufs.h"
#include "kernel/video.h"
#include "kernel/vmm.h"

#define SYS_IO_MAX_LEN 4096ULL
#define SYS_DRAW_ROW_MAX_LEN 256ULL
#define SYS_SPAWN_CMDLINE_MAX 128ULL
#define SYS_SPAWN_NAME_MAX 20ULL
#define SYS_OPEN_NAME_MAX 32ULL

static int user_range_ok(unsigned long long vaddr, unsigned long long len) {
  if (len == 0)
    return 1;

  if (len - 1 > ~0ULL - vaddr)
    return 0;

  vmm_address_space_t space = vmm_current_address_space();
  unsigned long long start = vaddr & ~0xFFFULL;
  unsigned long long end = (vaddr + len - 1) & ~0xFFFULL;

  for (unsigned long long page = start;; page += 0x1000) {
    if (!vmm_is_user_accessible_in(space, page))
      return 0;
    if (page == end)
      break;
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

  case SYS_REMOVE: {
    unsigned long long name_ptr = regs->rdi;

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

    regs->rax = (unsigned long long)ufs_delete(name);
    break;
  }

  case SYS_CLEAR: {
    clearwin();
    regs->rax = 0;
    break;
  }

  case SYS_SET_CURSOR: {
    unsigned long long row = regs->rsi;
    unsigned long long col = regs->rdx;
    video_set_cursor((unsigned int)row, (unsigned int)col);
    regs->rax = 0;
    break;
  }

  case SYS_DRAW_ROW: {
    unsigned long long row = regs->rdi;
    unsigned long long buf = regs->rsi;
    unsigned long long len = regs->rdx;

    if (len > SYS_DRAW_ROW_MAX_LEN || !user_range_ok(buf, len)) {
      regs->rax = (unsigned long long)-1;
      break;
    }

    char line[SYS_DRAW_ROW_MAX_LEN];
    const char *src = (const char *)buf;
    for (unsigned long long i = 0; i < len; i++)
      line[i] = src[i];

    video_draw_row((unsigned int)row, line, (unsigned int)len);
    regs->rax = 0;
    break;
  }

  case SYS_SCREEN_INFO: {
    unsigned long long cols_ptr = regs->rsi;
    unsigned long long rows_ptr = regs->rdx;

    if (!user_range_ok(cols_ptr, sizeof(unsigned long long)) ||
        !user_range_ok(rows_ptr, sizeof(unsigned long long))) {
      regs->rax = (unsigned long long)-1;
      break;
    }

    *(unsigned long long *)cols_ptr = video_cols();
    *(unsigned long long *)rows_ptr = video_rows();
    regs->rax = 0;
    break;
  }

  case SYS_LIST: {
    unsigned long long index = regs->rdi;
    unsigned long long buf = regs->rsi;
    unsigned long long buf_len = regs->rdx;

    if (buf_len == 0 || !user_range_ok(buf, buf_len)) {
      regs->rax = (unsigned long long)-1;
      break;
    }

    unsigned int disk_count = fs_file_count();
    unsigned int mem_count = memfs_file_count();
    const char *name = 0;

    if (index < disk_count) {
      const fs_dirent_t *e = fs_entry((unsigned int)index);
      name = e ? e->name : 0;
    } else if (index < (unsigned long long)disk_count + mem_count) {
      unsigned long long memfs_index = index - disk_count;
      int handle = memfs_first();
      unsigned long long i = 0;
      while (handle >= 0 && i < memfs_index) {
        handle = memfs_next(handle);
        i++;
      }
      if (handle >= 0)
        name = memfs_name(handle);
    } else {
      unsigned long long ufs_index = index - disk_count - mem_count;
      const ufs_dirent_t *e = ufs_entry((unsigned int)ufs_index);
      name = e ? e->name : 0;
    }

    if (name == 0) {
      regs->rax = (unsigned long long)-1;
      break;
    }

    char *dst = (char *)buf;
    unsigned long long i = 0;
    for (; i < buf_len - 1 && name[i] != '\0'; i++) {
      dst[i] = name[i];
    }
    dst[i] = '\0';

    regs->rax = 0;
    break;
  }

  case SYS_WHOAMI:
    /* System calls are entered from ring 3, so this identifies the caller. */
    regs->rax = (unsigned long long)sched_current_tid();
    break;

  case SYS_MEMINFO: {
    /* syscall2 uses rsi and rdx for its two payload arguments. */
    unsigned long long total_ptr = regs->rsi;
    unsigned long long free_ptr = regs->rdx;
    if (!user_range_ok(total_ptr, sizeof(unsigned long long)) ||
        !user_range_ok(free_ptr, sizeof(unsigned long long))) {
      regs->rax = (unsigned long long)-1;
      break;
    }

    *(unsigned long long *)total_ptr = pmm_total_frames();
    *(unsigned long long *)free_ptr = pmm_free_frames();
    regs->rax = 0;
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
    int status;
    if (process_wait((int)regs->rdi, &status) != 0)
      regs->rax = (unsigned long long)-1;
    else
      regs->rax = (unsigned long long)status;
    break;
  }

  case SYS_EXIT:
    klog_write("syscall: ring-3 thread exiting via SYS_EXIT\n");
    process_exit((int)regs->rdi);
    break;

  default:
    klog_write("syscall: unknown number ");
    klog_write_hex(regs->rax);
    klog_write(" from ring 3\n");
    regs->rax = (unsigned long long)-1;
    break;
  }
}
