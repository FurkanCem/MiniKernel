#include "shell_internal.h"
#include "kernel/fs.h"
#include "kernel/heap.h"
#include "kernel/process.h"
#include "kernel/thread.h"
#include "kernel/video.h"

void cmd_usermodetest(void) {
  print_str("\nspawning a ring-3 thread - check the serial/klog output "
            "for its syscall message:\n");

  int tid = process_spawn_demo_entry();
  if (tid < 0) {
    print_str("failed to spawn demo thread");
    return;
  }

  sched_wait(tid);

  print_str("\nring-3 thread exited back through SYS_EXIT");
}

void cmd_elftest(void) {
  print_str("\nloading and running a standalone ELF binary in its own "
            "address space - check the serial/klog output:\n");

  int tid = process_spawn_builtin_demo();
  if (tid < 0) {
    print_str("failed to spawn elf demo thread");
    return;
  }

  sched_wait(tid);

  print_str("\nelf process exited back through SYS_EXIT");
}

void cmd_ls(void) {
  unsigned int count = fs_file_count();
  if (count == 0) {
    print_str("\nno filesystem mounted or no files present");
    return;
  }

  for (unsigned int i = 0; i < count; i++) {
    const fs_dirent_t *e = fs_entry(i);
    if (e == 0)
      continue;
    print_str("\n");
    print_str(e->name);
  }
}

void cmd_run(const char *name) {
  const fs_dirent_t *entry = fs_find(name);
  if (entry == 0) {
    print_str("\nfile not found: ");
    print_str(name);
    return;
  }

  void *buf = kmalloc(entry->size_bytes);
  if (buf == 0) {
    print_str("\nout of memory reading file");
    return;
  }

  if (fs_read_file(entry, buf) != 0) {
    print_str("\nfailed to read file from disk");
    kfree(buf);
    return;
  }

  print_str("\nloaded ");
  print_str(name);
  print_str(" from disk, running it - check serial/klog output:\n");

  int tid = process_spawn_from_elf((const unsigned char *)buf,
                                    entry->size_bytes);
  if (tid < 0) {
    print_str("failed to spawn thread");
    kfree(buf);
    return;
  }

  sched_wait(tid);

  kfree(buf);
  print_str("\nprocess exited back through SYS_EXIT");
}
