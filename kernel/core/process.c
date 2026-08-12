#include "kernel/process.h"
#include "kernel/elf.h"
#include "kernel/elf_builtin.h"
#include "kernel/fs.h"
#include "kernel/heap.h"
#include "kernel/pmm.h"
#include "kernel/thread.h"
#include "kernel/vmm.h"
#include "kernel/vmm_stack.h"

extern void enter_usermode(void (*entry)(void), void *user_stack_top,
                            unsigned long long arg0, unsigned long long arg1);
extern void user_demo_entry(void);

#define USER_STACK_VADDR 0x8000100000ULL
#define USER_STACK_MAX_SIZE (1ULL * 1024 * 1024)
#define USER_ARGV_RESERVED 128ULL

typedef struct {
  const unsigned char *elf_data;
  unsigned long long elf_size;
  int owns_data;
  char argv[USER_ARGV_RESERVED];
  unsigned long long argv_len;
} elf_spawn_request_t;

static elf_spawn_request_t *pending_elf_request = 0;

static int setup_user_stack(vmm_address_space_t space, const char *argv_str,
                             unsigned long long argv_len,
                             unsigned long long *out_stack_top,
                             unsigned long long *out_argv_uaddr,
                             unsigned long long *out_argv_len) {
  unsigned long long stack_ceiling = USER_STACK_VADDR + USER_STACK_MAX_SIZE;
  unsigned long long first_page =
      (stack_ceiling - PMM_FRAME_SIZE) & ~(PMM_FRAME_SIZE - 1);

  unsigned long long frame = pmm_alloc_frame();
  if (frame == 0)
    return -1;

  if (vmm_map_page_in(space, first_page, frame, VMM_WRITABLE | VMM_USER) !=
      0) {
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
  elf_spawn_request_t *request = pending_elf_request;
  pending_elf_request = 0;

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
    sched_exit();
  }

  sched_set_address_space(sched_current_tid(), space);

  unsigned long long entry;
  int load_result = elf_load(space, elf_data, elf_size, &entry);

  if (owns_data)
    kfree((void *)elf_data);

  if (load_result != 0) {
    sched_exit();
  }

  unsigned long long stack_top, argv_uaddr, argv_actual_len;
  if (setup_user_stack(space, argv, argv_len, &stack_top, &argv_uaddr,
                        &argv_actual_len) != 0) {
    sched_exit();
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

  pending_elf_request = request;

  int tid = sched_spawn(elf_process_trampoline);
  if (tid < 0) {
    pending_elf_request = 0;
    if (owns_data)
      kfree((void *)elf_data);
    kfree(request);
  }

  return tid;
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

int process_spawn_builtin_demo(void) {
  return process_spawn_from_elf(tiny_elf_binary, tiny_elf_binary_size);
}

static void raw_entry_trampoline(void) {
  unsigned long long user_stack_frame = pmm_alloc_frame();
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
