#include "kernel/shell.h"
#include "kernel/e820.h"
#include "kernel/elf.h"
#include "kernel/fs.h"
#include "kernel/heap.h"
#include "kernel/keyboard.h"
#include "kernel/klog.h"
#include "kernel/pmm.h"
#include "kernel/thread.h"
#include "kernel/video.h"
#include "kernel/vmm.h"

extern void enter_usermode(void (*entry)(void), void *user_stack_top);
extern void user_demo_entry(void);

#define LINE_BUFFER_SIZE 128
static char line_buffer[LINE_BUFFER_SIZE];
static unsigned int line_length = 0;

static int str_eq(const char *a, const char *b) {
  unsigned int i = 0;
  while (a[i] != '\0' && b[i] != '\0') {
    if (a[i] != b[i])
      return 0;
    i++;
  }
  return a[i] == '\0' && b[i] == '\0';
}

static int str_starts_with(const char *str, const char *prefix) {
  unsigned int i = 0;
  while (prefix[i] != '\0') {
    if (str[i] != prefix[i])
      return 0;
    i++;
  }
  return 1;
}

static void print_str(const char *str) {
  for (unsigned int i = 0; str[i] != '\0'; i++) {
    putchar_at_cursor(str[i]);
  }
}

static void print_hex(unsigned long long value) {
  static const char digits[] = "0123456789ABCDEF";
  char buf[19]; /* "0x" + 16 hex digits + NUL */

  buf[0] = '0';
  buf[1] = 'x';
  for (int i = 0; i < 16; i++) {
    buf[2 + i] = digits[(value >> ((15 - i) * 4)) & 0xF];
  }
  buf[18] = '\0';

  print_str(buf);
}

static void print_meminfo(void) {
  unsigned int count = e820_entry_count();

  print_str("\nBIOS memory map (");
  print_hex(count);
  print_str(" regions):");

  for (unsigned int i = 0; i < count; i++) {
    const e820_entry_t *entry = e820_get_entry(i);
    if (entry == 0)
      break;

    print_str("\n  ");
    print_hex(entry->base);
    print_str(" - ");
    print_hex(entry->base + entry->length);
    print_str("  ");
    print_str(e820_type_name(entry->type));
  }

  print_str("\ntotal usable: ");
  print_hex(e820_total_usable_bytes());
  print_str(" bytes");

  print_str("\n\nframe allocator: ");
  print_hex(pmm_total_frames());
  print_str(" total frames, ");
  print_hex(pmm_free_frames());
  print_str(" free (");
  print_hex(pmm_total_frames() - pmm_free_frames());
  print_str(" used)");
}

static void print_alloctest(void) {
  print_str("\nallocating 3 frames:");

  for (int i = 0; i < 3; i++) {
    unsigned long long addr = pmm_alloc_frame();
    print_str("\n  frame ");
    print_hex((unsigned long long)i);
    print_str(": ");
    if (addr == 0) {
      print_str("out of memory");
    } else {
      print_hex(addr);
    }
  }

  print_str("\nfree frames remaining: ");
  print_hex(pmm_free_frames());
}

static void print_vmmtest(void) {
  print_str("\nallocating frames until one lands past the 2MB mark...");

  unsigned long long addr = 0;
  unsigned long long attempts = 0;
  const unsigned long long TWO_MB = 0x200000ULL;
  const unsigned long long MAX_ATTEMPTS = 100000ULL;

  while (attempts < MAX_ATTEMPTS) {
    addr = pmm_alloc_frame();
    attempts++;
    if (addr == 0) {
      print_str("\nout of memory before reaching 2MB");
      return;
    }
    if (addr >= TWO_MB)
      break;
  }

  print_str("\ngot frame ");
  print_hex(addr);
  print_str(" after ");
  print_hex(attempts);
  print_str(" allocations");

  volatile unsigned char *ptr = (volatile unsigned char *)addr;
  unsigned char pattern = 0xA5;
  *ptr = pattern;
  unsigned char readback = *ptr;

  print_str("\nwrote 0xA5 to it, read back ");
  print_hex((unsigned long long)readback);
  if (readback == pattern) {
    print_str(" -> OK, paging extension works");
  } else {
    print_str(" -> MISMATCH");
  }
}

static void print_heaptest(void) {
  print_str("\nallocating three blocks and writing through them:");

  char *a = (char *)kmalloc(32);
  char *b = (char *)kmalloc(64);
  char *c = (char *)kmalloc(16);

  if (a == 0 || b == 0 || c == 0) {
    print_str("\nkmalloc returned NULL - out of memory");
    return;
  }

  for (int i = 0; i < 31; i++)
    a[i] = 'A';
  a[31] = '\0';
  for (int i = 0; i < 63; i++)
    b[i] = 'B';
  b[63] = '\0';
  for (int i = 0; i < 15; i++)
    c[i] = 'C';
  c[15] = '\0';

  print_str("\n  a=");
  print_hex((unsigned long long)a);
  print_str("\n  b=");
  print_hex((unsigned long long)b);
  print_str("\n  c=");
  print_hex((unsigned long long)c);

  print_str("\nfreeing b, then allocating a similar-sized block:");
  kfree(b);
  char *d = (char *)kmalloc(40);
  print_str("\n  d=");
  print_hex((unsigned long long)d);
  if (d == b) {
    print_str(" (same address as freed b - free list reused it)");
  } else {
    print_str(" (different address - grew instead of reusing b this time)");
  }

  print_str("\ndata still intact after all that: a[0]=");
  putchar_at_cursor(a[0]);
  print_str(" c[0]=");
  putchar_at_cursor(c[0]);
}

static volatile int demo_a_done = 0;
static volatile int demo_b_done = 0;
static void demo_thread(void) {
  int id = sched_current_tid();

  for (int i = 0; i < 20000000; i++) {
    if (i % (400 * id) == 0)
      putchar_at_cursor('A' + id);
  }

  sched_exit();
}
static void demo_thread_a(void) {
  for (int i = 0; i < 20000000; i++) {
    if (i % 200000 == 0)
      putchar_at_cursor('A');
  }
  demo_a_done = 1;
  sched_exit();
}

static void demo_thread_b(void) {
  for (int i = 0; i < 20000000; i++) {
    if (i % 200000 == 0)
      putchar_at_cursor('B');
  }
  demo_b_done = 1;
  sched_exit();
}

static void print_threadtest(void) {
  demo_a_done = 0;
  demo_b_done = 0;

  print_str("\nspawning two threads that print A/B and yield:\n");

  sched_spawn(demo_thread_a);
  sched_spawn(demo_thread_b);

  while (!demo_a_done || !demo_b_done) {
    sched_yield();
  }

  print_str("\nboth threads finished");
}

static void print_largeThreadtest(void) {
  int size = 8;
  int tids[8];

  for (int i = 0; i < size; i++) {
    tids[i] = sched_spawn(demo_thread);
  }

  for (;;) {
    int finished = 1;

    for (int i = 0; i < size; i++) {
      if (sched_is_alive(tids[i])) {
        finished = 0;
        break;
      }
    }

    if (finished)
      break;

    sched_yield();
  }

  print_str("\nthreads finished");
}

static void usermode_demo_thread(void) {
  unsigned long long user_stack_frame = pmm_alloc_frame();
  unsigned long long code_page =
      (unsigned long long)user_demo_entry & ~0xFFFULL;

  if (user_stack_frame == 0) {
    print_str("\nusermode: out of memory setting up ring 3");
    sched_exit();
  }

  vmm_address_space_t space = vmm_create_address_space();
  if (space == 0) {
    print_str("\nusermode: out of memory setting up ring 3");
    sched_exit();
  }

  sched_set_address_space(sched_current_tid(), space);

  vmm_mark_user(user_stack_frame);
  vmm_mark_user(code_page);
  vmm_mark_user(code_page + PMM_FRAME_SIZE);

  enter_usermode(user_demo_entry, (void *)(user_stack_frame + PMM_FRAME_SIZE));
}

static void print_usermodetest(void) {
  print_str("\nspawning a ring-3 thread - check the serial/klog output "
            "for its syscall message:\n");

  int tid = sched_spawn(usermode_demo_thread);
  if (tid < 0) {
    print_str("failed to spawn demo thread");
    return;
  }

  while (sched_is_alive(tid)) {
    sched_yield();
  }

  print_str("\nring-3 thread exited back through SYS_EXIT");
}

#define ELF_STACK_VADDR 0x8000100000ULL
#define ELF_STACK_MAX_SIZE                                                     \
  (1ULL * 1024 * 1024) /* 1 MiB ceiling, grown on demand */

static int setup_user_stack(vmm_address_space_t space,
                            unsigned long long *out_stack_top) {
  unsigned long long stack_top = ELF_STACK_VADDR + ELF_STACK_MAX_SIZE;
  unsigned long long first_page =
      (stack_top - PMM_FRAME_SIZE) & ~(PMM_FRAME_SIZE - 1);

  unsigned long long frame = pmm_alloc_frame();
  if (frame == 0)
    return -1;

  if (vmm_map_page_in(space, first_page, frame, VMM_WRITABLE | VMM_USER) != 0) {
    pmm_free_frame(frame);
    return -1;
  }

  vmm_register_growable_stack(space, stack_top, ELF_STACK_MAX_SIZE);

  *out_stack_top = stack_top;
  return 0;
}

static void elf_demo_thread(void) {
  vmm_address_space_t space = vmm_create_address_space();
  if (space == 0) {
    sched_exit();
  }

  sched_set_address_space(sched_current_tid(), space);

  unsigned long long entry;
  if (elf_load(space, tiny_elf_binary, tiny_elf_binary_size, &entry) != 0) {
    sched_exit();
  }

  unsigned long long stack_top;
  if (setup_user_stack(space, &stack_top) != 0) {
    sched_exit();
  }

  enter_usermode((void (*)(void))entry, (void *)stack_top);
}

static void print_elftest(void) {
  print_str("\nloading and running a standalone ELF binary in its own "
            "address space - check the serial/klog output:\n");

  int tid = sched_spawn(elf_demo_thread);
  if (tid < 0) {
    print_str("failed to spawn elf demo thread");
    return;
  }

  while (sched_is_alive(tid)) {
    sched_yield();
  }

  print_str("\nelf process exited back through SYS_EXIT");
}

static void print_ls(void) {
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

static const unsigned char *run_file_data;
static unsigned long long run_file_size;

static void run_file_thread(void) {
  vmm_address_space_t space = vmm_create_address_space();
  if (space == 0) {
    sched_exit();
  }

  sched_set_address_space(sched_current_tid(), space);

  unsigned long long entry;
  if (elf_load(space, run_file_data, run_file_size, &entry) != 0) {
    sched_exit();
  }

  unsigned long long stack_top;
  if (setup_user_stack(space, &stack_top) != 0) {
    sched_exit();
  }

  enter_usermode((void (*)(void))entry, (void *)stack_top);
}

static void print_run(const char *name) {
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

  run_file_data = (const unsigned char *)buf;
  run_file_size = entry->size_bytes;

  print_str("\nloaded ");
  print_str(name);
  print_str(" from disk, running it - check serial/klog output:\n");

  int tid = sched_spawn(run_file_thread);
  if (tid < 0) {
    print_str("failed to spawn thread");
    kfree(buf);
    return;
  }

  while (sched_is_alive(tid)) {
    sched_yield();
  }

  kfree(buf);
  print_str("\nprocess exited back through SYS_EXIT");
}

static void print_prompt(void) { print_str("\n> "); }

static void run_command(const char *line) {
  klog_write("shell: command '");
  klog_write(line);
  klog_write("'\n");

  if (line[0] == '\0') {
    /* empty line - nothing to do, just reprint the prompt below */
  } else if (str_eq(line, "help")) {
    print_str("\ncommands: help, clear, echo <text>, meminfo, alloctest, "
              "vmmtest, heaptest, threadtest, largethreadtest, usermodetest, "
              "elftest, ls, run <name>");
  } else if (str_eq(line, "meminfo")) {
    print_meminfo();
  } else if (str_eq(line, "alloctest")) {
    print_alloctest();
  } else if (str_eq(line, "vmmtest")) {
    print_vmmtest();
  } else if (str_eq(line, "heaptest")) {
    print_heaptest();
  } else if (str_eq(line, "threadtest")) {
    print_threadtest();
  } else if (str_eq(line, "largethreadtest")) {
    print_largeThreadtest();
  } else if (str_eq(line, "usermodetest")) {
    print_usermodetest();
  } else if (str_eq(line, "elftest")) {
    print_elftest();
  } else if (str_eq(line, "ls")) {
    print_ls();
  } else if (str_starts_with(line, "run ")) {
    print_run(line + 4);
  } else if (str_eq(line, "clear")) {
    clearwin();
    video_reset_cursor();
    return; /* fresh screen already has the cursor in the right spot */
  } else if (str_starts_with(line, "echo ")) {
    print_str("\n");
    print_str(line + 5);
  } else {
    print_str("\nunknown command: ");
    print_str(line);
  }

  print_prompt();
}

__attribute__((noreturn)) void shell_run(void) {
  print_str("Type 'help' for a list of commands.");
  print_prompt();

  for (;;) {
    char c;

    if (!kbd_read_char(&c)) {
      __asm__ volatile("hlt"); /* nothing to do - wake on the next interrupt */
      continue;
    }

    if (c == '\n') {
      line_buffer[line_length] = '\0';
      run_command(line_buffer);
      line_length = 0;
    } else if (c == '\b') {
      if (line_length > 0) {
        line_length--;
        putchar_at_cursor('\b');
      }
    } else if (line_length < LINE_BUFFER_SIZE - 1) {
      line_buffer[line_length++] = c;
      putchar_at_cursor(c);
    }
    /* else: line full - drop further characters until Enter/Backspace */
  }
}
