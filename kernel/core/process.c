#include "kernel/process.h"
#include "kernel/elf.h"
#include "kernel/elf_builtin.h"
#include "kernel/fs.h"
#include "kernel/heap.h"
#include "kernel/io.h"
#include "kernel/pipe.h"
#include "kernel/pmm.h"
#include "kernel/thread.h"
#include "kernel/vmm.h"
#include "kernel/vmm_stack.h"

extern void enter_usermode(void (*entry)(void), void *user_stack_top,
                           unsigned long long arg0, unsigned long long arg1);
extern void user_demo_entry(void);

#define USER_PML4_SLOT_BASE 0x0000008000000000ULL
#define USER_PROCESS_SLOT_SIZE (16ULL * 1024 * 1024)
#define USER_IMAGE_LINK_BASE 0x0000000000400000ULL
#define USER_STACK_MAX_SIZE (1ULL * 1024 * 1024)
#define USER_ARGV_RESERVED 128ULL

typedef struct {
  const unsigned char *elf_data;
  unsigned long long elf_size;
  int owns_data;
  char argv[USER_ARGV_RESERVED];
  unsigned long long argv_len;
} elf_spawn_request_t;

#define PROCESS_MAX 64

typedef enum {
  PROCESS_UNUSED,
  PROCESS_STARTING,
  PROCESS_RUNNING,
  PROCESS_EXITED,
} process_state_t;

typedef struct {
  int pid;
  int tid;
  int parent_pid;
  int exit_status;
  process_state_t state;
  vmm_address_space_t address_space;
} process_t;

static process_t processes[PROCESS_MAX];
static int next_pid = 1;

static process_t *find_process(int pid) {
  for (int i = 0; i < PROCESS_MAX; i++) {
    if (processes[i].state != PROCESS_UNUSED && processes[i].pid == pid)
      return &processes[i];
  }
  return 0;
}

static process_t *find_process_by_tid(int tid) {
  for (int i = 0; i < PROCESS_MAX; i++) {
    if (processes[i].state != PROCESS_UNUSED && processes[i].tid == tid)
      return &processes[i];
  }
  return 0;
}

static int current_pid(void) {
  process_t *process = find_process_by_tid(sched_current_tid());
  return process ? process->pid : 0; /* kernel threads belong to PID 0 */
}

static int reserve_process(int *out_pid) {
  for (int i = 0; i < PROCESS_MAX; i++) {
    if (processes[i].state != PROCESS_UNUSED)
      continue;

    int pid = next_pid++;
    if (next_pid <= 0)
      next_pid = 1;

    processes[i].pid = pid;
    processes[i].tid = -1;
    processes[i].parent_pid = current_pid();
    processes[i].exit_status = -1;
    processes[i].state = PROCESS_STARTING;
    processes[i].address_space = 0;
    *out_pid = pid;
    return 0;
  }
  return -1;
}

static void release_process(int pid) {
  process_t *process = find_process(pid);
  if (process == 0)
    return;

  process->pid = 0;
  process->tid = -1;
  process->parent_pid = 0;
  process->exit_status = 0;
  process->state = PROCESS_UNUSED;
  process->address_space = 0;
}

static int user_process_layout(int tid, unsigned long long *out_image_base,
                               unsigned long long *out_stack_ceiling) {
  if (tid < 0)
    return -1;

  unsigned long long slot = (unsigned long long)tid * USER_PROCESS_SLOT_SIZE;
  if (slot / USER_PROCESS_SLOT_SIZE != (unsigned long long)tid ||
      slot > (1ULL << 39) - USER_PROCESS_SLOT_SIZE)
    return -1;

  *out_image_base = USER_PML4_SLOT_BASE + slot;
  *out_stack_ceiling = *out_image_base + USER_PROCESS_SLOT_SIZE;
  return 0;
}

static int setup_user_stack(vmm_address_space_t space,
                            unsigned long long stack_ceiling,
                            const char *argv_str, unsigned long long argv_len,
                            unsigned long long *out_stack_top,
                            unsigned long long *out_argv_uaddr,
                            unsigned long long *out_argv_len) {
  unsigned long long first_page =
      (stack_ceiling - PMM_FRAME_SIZE) & ~(PMM_FRAME_SIZE - 1);

  unsigned long long frame = pmm_alloc_zeroed_frame();
  if (frame == 0)
    return -1;

  if (vmm_map_page_in(space, first_page, frame, VMM_WRITABLE | VMM_USER) != 0) {
    pmm_free_frame(frame);
    return -1;
  }

  vmm_register_growable_stack(space, stack_ceiling, USER_STACK_MAX_SIZE);

  if (argv_len > USER_ARGV_RESERVED)
    argv_len = USER_ARGV_RESERVED;

  unsigned long long argv_page_offset = PMM_FRAME_SIZE - USER_ARGV_RESERVED;
  unsigned char *kview = (unsigned char *)(frame + argv_page_offset);
  for (unsigned long long i = 0; i < argv_len; i++) {
    kview[i] = (unsigned char)argv_str[i];
  }

  *out_stack_top = stack_ceiling - USER_ARGV_RESERVED;
  *out_argv_uaddr = first_page + argv_page_offset;
  *out_argv_len = argv_len;

  return 0;
}

static void elf_process_trampoline(void) {
  elf_spawn_request_t *request = (elf_spawn_request_t *)sched_current_context();
  sched_set_current_context(0);

  if (request == 0)
    process_exit(-1);

  const unsigned char *elf_data = request->elf_data;
  unsigned long long elf_size = request->elf_size;
  int owns_data = request->owns_data;

  char argv[USER_ARGV_RESERVED];
  unsigned long long argv_len = request->argv_len;
  for (unsigned long long i = 0; i < argv_len; i++) {
    argv[i] = request->argv[i];
  }

  kfree(request);

  vmm_address_space_t space = vmm_create_address_space();
  if (space == 0) {
    if (owns_data)
      kfree((void *)elf_data);
    process_exit(-1);
  }

  sched_set_address_space(sched_current_tid(), space);

  process_t *process = find_process_by_tid(sched_current_tid());
  if (process != 0) {
    process->address_space = space;
    process->state = PROCESS_RUNNING;
  }

  unsigned long long image_base, stack_ceiling;
  if (user_process_layout(sched_current_tid(), &image_base, &stack_ceiling) !=
      0) {
    if (owns_data)
      kfree((void *)elf_data);
    process_exit(-1);
  }

  unsigned long long entry;
  int load_result = elf_load(space, elf_data, elf_size,
                             image_base - USER_IMAGE_LINK_BASE, &entry);

  if (owns_data)
    kfree((void *)elf_data);

  if (load_result != 0) {
    process_exit(-1);
  }

  unsigned long long stack_top, argv_uaddr, argv_actual_len;
  if (setup_user_stack(space, stack_ceiling, argv, argv_len, &stack_top,
                       &argv_uaddr, &argv_actual_len) != 0) {
    process_exit(-1);
  }

  enter_usermode((void (*)(void))entry, (void *)stack_top, argv_uaddr,
                 argv_actual_len);
}

static int spawn_elf_request(const unsigned char *elf_data,
                             unsigned long long elf_size, int owns_data,
                             const char *argv_str,
                             unsigned long long argv_len) {
  elf_spawn_request_t *request =
      (elf_spawn_request_t *)kmalloc(sizeof(elf_spawn_request_t));
  if (request == 0) {
    if (owns_data)
      kfree((void *)elf_data);
    return -1;
  }

  request->elf_data = elf_data;
  request->elf_size = elf_size;
  request->owns_data = owns_data;

  if (argv_len > USER_ARGV_RESERVED)
    argv_len = USER_ARGV_RESERVED;
  request->argv_len = argv_len;
  for (unsigned long long i = 0; i < argv_len; i++) {
    request->argv[i] = argv_str[i];
  }

  unsigned long long flags = irq_save();
  int pid;
  if (reserve_process(&pid) != 0) {
    irq_restore(flags);
    if (owns_data)
      kfree((void *)elf_data);
    kfree(request);
    return -1;
  }

  int tid = sched_spawn_with_context(elf_process_trampoline, request);
  if (tid < 0) {
    release_process(pid);
    irq_restore(flags);
    if (owns_data)
      kfree((void *)elf_data);
    kfree(request);
    return -1;
  }

  process_t *process = find_process(pid);
  process->tid = tid;
  irq_restore(flags);
  return pid;
}

int process_spawn_from_elf(const unsigned char *elf_data,
                           unsigned long long elf_size) {
  return spawn_elf_request(elf_data, elf_size, 0, 0, 0);
}

int process_spawn_from_file(const char *name, const char *argv_str,
                            unsigned long long argv_len) {
  const fs_dirent_t *entry = fs_find(name);
  if (entry == 0)
    return -1;

  void *buf = kmalloc(entry->size_bytes);
  if (buf == 0)
    return -1;

  if (fs_read_file(entry, buf) != 0) {
    kfree(buf);
    return -1;
  }

  return spawn_elf_request((const unsigned char *)buf, entry->size_bytes, 1,
                           argv_str, argv_len);
}

int process_spawn_from_file_redirect(const char *name, const char *argv_str,
                                     unsigned long long argv_len, int stdin_fd,
                                     int stdout_fd) {
  unsigned long long flags = irq_save();

  int pid = process_spawn_from_file(name, argv_str, argv_len);
  if (pid < 0) {
    irq_restore(flags);
    return -1;
  }

  process_t *process = find_process(pid);
  int tid = (process != 0) ? process->tid : -1;

  if (tid >= 0 && stdin_fd >= 0) {
    fd_slot_t *src = sched_fd_get(stdin_fd);
    if (src != 0 && src->kind == FD_KIND_PIPE_READ) {
      pipe_add_reader(src->handle);
      sched_fd_set(tid, 0, FD_KIND_PIPE_READ, src->handle);
    }
  }

  if (tid >= 0 && stdout_fd >= 0) {
    fd_slot_t *src = sched_fd_get(stdout_fd);
    if (src != 0 && src->kind == FD_KIND_PIPE_WRITE) {
      pipe_add_writer(src->handle);
      sched_fd_set(tid, 1, FD_KIND_PIPE_WRITE, src->handle);
    }
  }

  irq_restore(flags);
  return pid;
}

int process_spawn_builtin_demo(void) {
  return process_spawn_from_elf(tiny_elf_binary, tiny_elf_binary_size);
}

int process_wait(int pid, int *out_status) {
  for (;;) {
    unsigned long long flags = irq_save();
    process_t *process = find_process(pid);
    if (process == 0 || process->parent_pid != current_pid()) {
      irq_restore(flags);
      return -1;
    }

    if (process->state == PROCESS_EXITED) {
      int status = process->exit_status;
      /* Reclaim the scheduler zombie before releasing its PID record. */
      if (process->tid >= 0)
        sched_reap_thread(process->tid);
      release_process(pid);
      irq_restore(flags);
      if (out_status != 0)
        *out_status = status;
      return 0;
    }

    /* Keep interrupts disabled until sched_sleep marks us blocked. */
    sched_sleep((const void *)(unsigned long long)pid);
    irq_restore(flags);
  }
}

void process_exit(int status) {
  sched_exit_status(status);
  __builtin_unreachable();
}

void process_mark_exited(int tid, int status) {
  process_t *process = find_process_by_tid(tid);
  if (process == 0)
    return;

  process->exit_status = status;
  process->state = PROCESS_EXITED;
  sched_wakeup((const void *)(unsigned long long)process->pid);
}

#define SIGNAL_EXIT_STATUS(sig) (-128 - (sig))

int process_get_tid(int pid) {
  unsigned long long flags = irq_save();
  process_t *process = find_process(pid);
  int tid =
      (process != 0 && process->state != PROCESS_EXITED) ? process->tid : -1;
  irq_restore(flags);
  return tid;
}

int process_kill(int pid, int sig) {
  unsigned long long flags = irq_save();
  process_t *process = find_process(pid);
  int tid =
      (process != 0 && process->state != PROCESS_EXITED) ? process->tid : -1;
  irq_restore(flags);

  if (tid < 0)
    return -1;

  return sched_kill(tid, SIGNAL_EXIT_STATUS(sig));
}

void process_reap_thread(int tid, vmm_address_space_t space) {
  process_t *process = find_process_by_tid(tid);
  if (process != 0)
    process->address_space = 0;
  vmm_destroy_address_space(space);
}

static void raw_entry_trampoline(void) {
  unsigned long long user_stack_frame = pmm_alloc_zeroed_frame();
  unsigned long long code_page =
      (unsigned long long)user_demo_entry & ~0xFFFULL;

  if (user_stack_frame == 0) {
    sched_exit();
  }

  vmm_address_space_t space = vmm_create_address_space();
  if (space == 0) {
    sched_exit();
  }

  sched_set_address_space(sched_current_tid(), space);

  vmm_mark_user(user_stack_frame);
  vmm_mark_user(code_page);
  vmm_mark_user(code_page + PMM_FRAME_SIZE);

  enter_usermode(user_demo_entry, (void *)(user_stack_frame + PMM_FRAME_SIZE),
                 0, 0);
}

int process_spawn_demo_entry(void) { return sched_spawn(raw_entry_trampoline); }
