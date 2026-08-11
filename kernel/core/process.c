#include "kernel/process.h"
#include "kernel/elf.h"
#include "kernel/elf_builtin.h"
#include "kernel/heap.h"
#include "kernel/pmm.h"
#include "kernel/thread.h"
#include "kernel/vmm.h"
#include "kernel/vmm_stack.h"

extern void enter_usermode(void (*entry)(void), void *user_stack_top);
extern void user_demo_entry(void);

#define USER_STACK_VADDR 0x8000100000ULL
#define USER_STACK_MAX_SIZE (1ULL * 1024 * 1024)

typedef struct {
  const unsigned char *elf_data;
  unsigned long long elf_size;
} elf_spawn_request_t;

static elf_spawn_request_t *pending_elf_request = 0;

static int setup_user_stack(vmm_address_space_t space,
                             unsigned long long *out_stack_top) {
  unsigned long long stack_top = USER_STACK_VADDR + USER_STACK_MAX_SIZE;
  unsigned long long first_page =
      (stack_top - PMM_FRAME_SIZE) & ~(PMM_FRAME_SIZE - 1);

  unsigned long long frame = pmm_alloc_frame();
  if (frame == 0)
    return -1;

  if (vmm_map_page_in(space, first_page, frame, VMM_WRITABLE | VMM_USER) !=
      0) {
    pmm_free_frame(frame);
    return -1;
  }

  vmm_register_growable_stack(space, stack_top, USER_STACK_MAX_SIZE);

  *out_stack_top = stack_top;
  return 0;
}

static void elf_process_trampoline(void) {
  elf_spawn_request_t *request = pending_elf_request;
  pending_elf_request = 0;

  const unsigned char *elf_data = request->elf_data;
  unsigned long long elf_size = request->elf_size;
  kfree(request);

  vmm_address_space_t space = vmm_create_address_space();
  if (space == 0) {
    sched_exit();
  }

  sched_set_address_space(sched_current_tid(), space);

  unsigned long long entry;
  if (elf_load(space, elf_data, elf_size, &entry) != 0) {
    sched_exit();
  }

  unsigned long long stack_top;
  if (setup_user_stack(space, &stack_top) != 0) {
    sched_exit();
  }

  enter_usermode((void (*)(void))entry, (void *)stack_top);
}

int process_spawn_from_elf(const unsigned char *elf_data,
                            unsigned long long elf_size) {
  elf_spawn_request_t *request =
      (elf_spawn_request_t *)kmalloc(sizeof(elf_spawn_request_t));
  if (request == 0)
    return -1;

  request->elf_data = elf_data;
  request->elf_size = elf_size;
  pending_elf_request = request;

  int tid = sched_spawn(elf_process_trampoline);
  if (tid < 0) {
    pending_elf_request = 0;
    kfree(request);
  }

  return tid;
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

  enter_usermode(user_demo_entry, (void *)(user_stack_frame + PMM_FRAME_SIZE));
}

int process_spawn_demo_entry(void) { return sched_spawn(raw_entry_trampoline); }
