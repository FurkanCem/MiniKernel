#include "kernel/thread.h"
#include "kernel/gdt.h"
#include "kernel/io.h"
#include "kernel/pmm.h"
#include "kernel/vmm.h"

#define MAX_THREADS 8
#define THREAD_STACK_PAGES 2
#define THREAD_STACK_SIZE (THREAD_STACK_PAGES * PMM_FRAME_SIZE)

typedef enum {
  THREAD_UNUSED,
  THREAD_READY,
  THREAD_RUNNING,
  THREAD_ZOMBIE
} thread_state_t;

static const int quantum_ticks[THREAD_PRIO_COUNT] = {
    [THREAD_PRIO_LOW] = 2,
    [THREAD_PRIO_NORMAL] = 4,
    [THREAD_PRIO_HIGH] = 8,
};

typedef struct {
  unsigned long long rsp;
  thread_state_t state;
  unsigned long long stack_low;
  unsigned long long guard_page;
  thread_priority_t priority;
  int ticks_remaining;
  vmm_address_space_t address_space;
} thread_t;

static thread_t threads[MAX_THREADS];
static int current_thread = -1;
static int zombie_slot = -1;

extern void switch_context(unsigned long long *old_rsp,
                           unsigned long long *new_rsp);

static void reap_zombie(void) {
  if (zombie_slot == -1)
    return;

  if (threads[zombie_slot].guard_page != 0) {
    for (unsigned long long i = 0; i < THREAD_STACK_PAGES; i++) {
      pmm_free_frame(threads[zombie_slot].stack_low + i * PMM_FRAME_SIZE);
    }
    vmm_map_page(threads[zombie_slot].guard_page,
                 threads[zombie_slot].guard_page, VMM_WRITABLE);
    pmm_free_frame(threads[zombie_slot].guard_page);
  }

  threads[zombie_slot].state = THREAD_UNUSED;
  threads[zombie_slot].stack_low = 0;
  threads[zombie_slot].guard_page = 0;
  threads[zombie_slot].rsp = 0;
  threads[zombie_slot].address_space = 0;
  zombie_slot = -1;
}

static int find_next_runnable(int from) {
  int next = from;
  for (int i = 0; i < MAX_THREADS; i++) {
    next = (next + 1) % MAX_THREADS;
    if (threads[next].state == THREAD_READY ||
        threads[next].state == THREAD_RUNNING)
      break;
  }
  return next;
}

static int alloc_guarded_stack(unsigned long long *out_stack_low,
                               unsigned long long *out_guard_page) {
  unsigned long long region = pmm_alloc_contiguous(THREAD_STACK_PAGES + 1);
  if (region == 0)
    return -1;

  unsigned long long guard_page = region;
  unsigned long long stack_low = region + PMM_FRAME_SIZE;

  vmm_guard_page(guard_page);
  if (vmm_is_mapped(guard_page)) {
    for (unsigned long long i = 0; i < THREAD_STACK_PAGES + 1; i++)
      pmm_free_frame(region + i * PMM_FRAME_SIZE);
    return -1;
  }

  *out_stack_low = stack_low;
  *out_guard_page = guard_page;
  return 0;
}

static void activate(int tid) {
  tss_set_kernel_stack(threads[tid].stack_low + THREAD_STACK_SIZE);
  vmm_switch_address_space(threads[tid].address_space);
}

void sched_init(void) {
  for (int i = 0; i < MAX_THREADS; i++) {
    threads[i].state = THREAD_UNUSED;
    threads[i].stack_low = 0;
    threads[i].guard_page = 0;
    threads[i].rsp = 0;
    threads[i].priority = THREAD_PRIO_NORMAL;
    threads[i].ticks_remaining = quantum_ticks[THREAD_PRIO_NORMAL];
    threads[i].address_space = 0;
  }

  unsigned long long stack_low = 0, guard_page = 0;
  alloc_guarded_stack(&stack_low, &guard_page);

  threads[0].state = THREAD_RUNNING;
  threads[0].address_space = vmm_kernel_address_space();
  threads[0].stack_low = stack_low;
  threads[0].guard_page = guard_page;
  current_thread = 0;
  zombie_slot = -1;

  activate(0);
}

int sched_spawn_prio(thread_entry_fn entry, thread_priority_t priority) {
  if (priority < 0 || priority >= THREAD_PRIO_COUNT)
    priority = THREAD_PRIO_NORMAL;

  int slot = -1;
  for (int i = 0; i < MAX_THREADS; i++) {
    if (threads[i].state == THREAD_UNUSED) {
      slot = i;
      break;
    }
  }
  if (slot == -1)
    return -1;

  unsigned long long stack_low, guard_page;
  if (alloc_guarded_stack(&stack_low, &guard_page) != 0)
    return -1;

  unsigned long long *sp =
      (unsigned long long *)(stack_low + THREAD_STACK_SIZE);

  sp -= 1;
  *sp = (unsigned long long)entry;
  sp -= 1;
  *sp = 0x202;
  sp -= 1;
  *sp = 0;
  sp -= 1;
  *sp = 0;
  sp -= 1;
  *sp = 0;
  sp -= 1;
  *sp = 0;
  sp -= 1;
  *sp = 0;
  sp -= 1;
  *sp = 0;

  threads[slot].rsp = (unsigned long long)sp;
  threads[slot].state = THREAD_READY;
  threads[slot].stack_low = stack_low;
  threads[slot].guard_page = guard_page;
  threads[slot].priority = priority;
  threads[slot].ticks_remaining = quantum_ticks[priority];
  threads[slot].address_space = vmm_kernel_address_space();

  return slot;
}

int sched_spawn(thread_entry_fn entry) {
  return sched_spawn_prio(entry, THREAD_PRIO_NORMAL);
}

void sched_set_address_space(int tid, vmm_address_space_t space) {
  if (tid < 0 || tid >= MAX_THREADS)
    return;

  threads[tid].address_space = space;
  if (tid == current_thread)
    vmm_switch_address_space(space);
}

void sched_yield(void) {
  if (current_thread == -1)
    return;

  unsigned long long flags = irq_save();

  reap_zombie();

  int next = find_next_runnable(current_thread);
  if (next == current_thread) {
    irq_restore(flags);
    return;
  }

  threads[current_thread].state = THREAD_READY;
  threads[next].state = THREAD_RUNNING;
  threads[next].ticks_remaining = quantum_ticks[threads[next].priority];

  int prev = current_thread;
  current_thread = next;
  activate(next);

  switch_context(&threads[prev].rsp, &threads[next].rsp);

  irq_restore(flags);
}

void sched_tick(void) {
  if (current_thread == -1)
    return;

  if (--threads[current_thread].ticks_remaining > 0)
    return;

  sched_yield();
}

void sched_set_priority(int tid, thread_priority_t priority) {
  if (tid < 0 || tid >= MAX_THREADS)
    return;
  if (priority < 0 || priority >= THREAD_PRIO_COUNT)
    return;

  threads[tid].priority = priority;
}

void sched_exit(void) {
  if (current_thread == -1)
    return;

  unsigned long long flags = irq_save();

  reap_zombie();

  threads[current_thread].state = THREAD_ZOMBIE;
  zombie_slot = current_thread;

  int next = find_next_runnable(current_thread);
  if (next == current_thread) {
    irq_restore(flags);
    for (;;)
      __asm__ volatile("hlt");
  }

  threads[next].state = THREAD_RUNNING;
  current_thread = next;
  activate(next);

  unsigned long long discard;
  switch_context(&discard, &threads[next].rsp);

  irq_restore(flags);
}

int sched_current_tid(void) { return current_thread; }
int sched_is_alive(int tid) {
  if (tid < 0 || tid >= MAX_THREADS)
    return 0;

  return threads[tid].state == THREAD_READY ||
         threads[tid].state == THREAD_RUNNING;
}
