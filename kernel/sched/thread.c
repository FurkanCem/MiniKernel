#include "kernel/thread.h"
#include "kernel/gdt.h"
#include "kernel/heap.h"
#include "kernel/io.h"
#include "kernel/pmm.h"
#include "kernel/process.h"
#include "kernel/vmm.h"
#include "kernel/vmm_stack.h"

#define INITIAL_THREAD_CAPACITY 4
#define THREAD_STACK_PAGES 2
#define THREAD_STACK_SIZE (THREAD_STACK_PAGES * PMM_FRAME_SIZE)

typedef enum {
  THREAD_UNUSED,
  THREAD_READY,
  THREAD_RUNNING,
  THREAD_BLOCKED,
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
  const void *wait_channel;
  void *context;
  fd_slot_t fds[THREAD_MAX_FDS];
} thread_t;

static thread_t *threads = 0;
static int thread_capacity = 0;
static int current_thread = -1;

extern void switch_context(unsigned long long *old_rsp,
                           unsigned long long *new_rsp);

static void reset_fd_slot(fd_slot_t *fd) {
  fd->kind = FD_KIND_UNUSED;
  fd->handle = -1;
  fd->cursor = 0;
}

static void init_thread_slot(thread_t *t) {
  t->state = THREAD_UNUSED;
  t->stack_low = 0;
  t->guard_page = 0;
  t->rsp = 0;
  t->priority = THREAD_PRIO_NORMAL;
  t->ticks_remaining = quantum_ticks[THREAD_PRIO_NORMAL];
  t->address_space = 0;
  t->wait_channel = 0;
  t->context = 0;
  for (int i = 0; i < THREAD_MAX_FDS; i++) {
    reset_fd_slot(&t->fds[i]);
  }
}

static void copy_thread_slot(thread_t *dst, const thread_t *src) {
  dst->rsp = src->rsp;
  dst->state = src->state;
  dst->stack_low = src->stack_low;
  dst->guard_page = src->guard_page;
  dst->priority = src->priority;
  dst->ticks_remaining = src->ticks_remaining;
  dst->address_space = src->address_space;
  dst->wait_channel = src->wait_channel;
  dst->context = src->context;
  for (int i = 0; i < THREAD_MAX_FDS; i++) {
    dst->fds[i].kind = src->fds[i].kind;
    dst->fds[i].handle = src->fds[i].handle;
    dst->fds[i].cursor = src->fds[i].cursor;
  }
}

static int grow_thread_table(void) {
  int new_capacity =
      thread_capacity == 0 ? INITIAL_THREAD_CAPACITY : thread_capacity * 2;

  thread_t *new_table =
      (thread_t *)kmalloc(sizeof(thread_t) * (unsigned long long)new_capacity);
  if (new_table == 0)
    return -1;

  unsigned long long flags = irq_save();

  for (int i = 0; i < thread_capacity; i++)
    copy_thread_slot(&new_table[i], &threads[i]);
  for (int i = thread_capacity; i < new_capacity; i++)
    init_thread_slot(&new_table[i]);

  thread_t *old_table = threads;
  threads = new_table;
  thread_capacity = new_capacity;

  irq_restore(flags);

  if (old_table != 0)
    kfree(old_table);

  return 0;
}

static void reap_zombie(int target_tid) {
  int zombie_slot = target_tid;
  if (zombie_slot < 0 || zombie_slot >= thread_capacity ||
      zombie_slot == current_thread ||
      threads[zombie_slot].state != THREAD_ZOMBIE)
    return;

  if (threads[zombie_slot].guard_page != 0) {
    for (unsigned long long i = 0; i < THREAD_STACK_PAGES; i++) {
      pmm_free_frame(threads[zombie_slot].stack_low + i * PMM_FRAME_SIZE);
    }
    vmm_map_page(threads[zombie_slot].guard_page,
                 threads[zombie_slot].guard_page, VMM_WRITABLE);
    pmm_free_frame(threads[zombie_slot].guard_page);
  }

  if (threads[zombie_slot].address_space != vmm_kernel_address_space()) {
    vmm_unregister_growable_stack(threads[zombie_slot].address_space);
    process_reap_thread(zombie_slot, threads[zombie_slot].address_space);
  }

  threads[zombie_slot].state = THREAD_UNUSED;
  threads[zombie_slot].stack_low = 0;
  threads[zombie_slot].guard_page = 0;
  threads[zombie_slot].rsp = 0;
  threads[zombie_slot].address_space = 0;
  threads[zombie_slot].wait_channel = 0;
  threads[zombie_slot].context = 0;
  for (int i = 0; i < THREAD_MAX_FDS; i++) {
    reset_fd_slot(&threads[zombie_slot].fds[i]);
  }
}

void sched_reap_thread(int tid) {
  unsigned long long flags = irq_save();
  reap_zombie(tid);
  irq_restore(flags);
}

static int find_next_runnable(int from) {
  int next = from;
  for (int i = 0; i < thread_capacity; i++) {
    next = (next + 1) % thread_capacity;
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
  grow_thread_table();

  unsigned long long stack_low = 0, guard_page = 0;
  alloc_guarded_stack(&stack_low, &guard_page);

  threads[0].state = THREAD_RUNNING;
  threads[0].address_space = vmm_kernel_address_space();
  threads[0].stack_low = stack_low;
  threads[0].guard_page = guard_page;
  current_thread = 0;

  activate(0);
}

static int sched_spawn_prio_with_context(thread_entry_fn entry,
                                         thread_priority_t priority,
                                         void *context) {
  if (priority < 0 || priority >= THREAD_PRIO_COUNT)
    priority = THREAD_PRIO_NORMAL;

  int slot = -1;
  for (int i = 0; i < thread_capacity; i++) {
    if (threads[i].state == THREAD_UNUSED) {
      slot = i;
      break;
    }
  }

  if (slot == -1) {
    int previous_capacity = thread_capacity;
    if (grow_thread_table() != 0)
      return -1;
    slot = previous_capacity;
  }

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
  threads[slot].context = context;

  return slot;
}

int sched_spawn_prio(thread_entry_fn entry, thread_priority_t priority) {
  return sched_spawn_prio_with_context(entry, priority, 0);
}

int sched_spawn(thread_entry_fn entry) {
  return sched_spawn_prio_with_context(entry, THREAD_PRIO_NORMAL, 0);
}

int sched_spawn_with_context(thread_entry_fn entry, void *context) {
  return sched_spawn_prio_with_context(entry, THREAD_PRIO_NORMAL, context);
}

void sched_set_address_space(int tid, vmm_address_space_t space) {
  if (tid < 0 || tid >= thread_capacity)
    return;

  threads[tid].address_space = space;
  if (tid == current_thread)
    vmm_switch_address_space(space);
}

void sched_yield(void) {
  if (current_thread == -1)
    return;

  unsigned long long flags = irq_save();

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

void sched_sleep(const void *channel) {
  unsigned long long flags = irq_save();

  threads[current_thread].wait_channel = channel;
  threads[current_thread].state = THREAD_BLOCKED;

  int next = find_next_runnable(current_thread);

  if (next == current_thread) {
    __asm__ volatile("sti");

    while (threads[current_thread].state == THREAD_BLOCKED) {
      __asm__ volatile("hlt");
    }

    threads[current_thread].state = THREAD_RUNNING;
    irq_restore(flags);
    return;
  }

  int prev = current_thread;
  threads[next].state = THREAD_RUNNING;
  threads[next].ticks_remaining = quantum_ticks[threads[next].priority];
  current_thread = next;
  activate(next);

  switch_context(&threads[prev].rsp, &threads[next].rsp);

  irq_restore(flags);
}

void sched_wakeup(const void *channel) {
  unsigned long long flags = irq_save();

  for (int i = 0; i < thread_capacity; i++) {
    if (threads[i].state == THREAD_BLOCKED &&
        threads[i].wait_channel == channel) {
      threads[i].state = THREAD_READY;
      threads[i].wait_channel = 0;
    }
  }

  irq_restore(flags);
}

void sched_wait(int tid) {
  while (sched_is_alive(tid)) {
    sched_sleep((const void *)(unsigned long long)tid);
  }
}

int sched_fd_open(int kind, int handle) {
  if (current_thread == -1)
    return -1;

  thread_t *t = &threads[current_thread];
  for (int i = 2; i < THREAD_MAX_FDS; i++) {
    if (t->fds[i].kind == FD_KIND_UNUSED) {
      t->fds[i].kind = kind;
      t->fds[i].handle = handle;
      t->fds[i].cursor = 0;
      return i;
    }
  }

  return -1;
}

int sched_fd_close(int fd) {
  if (current_thread == -1 || fd < 0 || fd >= THREAD_MAX_FDS)
    return -1;

  reset_fd_slot(&threads[current_thread].fds[fd]);
  return 0;
}

fd_slot_t *sched_fd_get(int fd) {
  if (current_thread == -1 || fd < 0 || fd >= THREAD_MAX_FDS)
    return 0;

  if (threads[current_thread].fds[fd].kind == FD_KIND_UNUSED)
    return 0;

  return &threads[current_thread].fds[fd];
}

void sched_set_priority(int tid, thread_priority_t priority) {
  if (tid < 0 || tid >= thread_capacity)
    return;
  if (priority < 0 || priority >= THREAD_PRIO_COUNT)
    return;

  threads[tid].priority = priority;
}

void sched_exit(void) { sched_exit_status(-1); }

void sched_exit_status(int status) {
  if (current_thread == -1)
    return;

  unsigned long long flags = irq_save();

  threads[current_thread].state = THREAD_ZOMBIE;

  /* Publish process exit only after this thread is no longer runnable. */
  process_mark_exited(current_thread, status);
  sched_wakeup((const void *)(unsigned long long)current_thread);

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
void *sched_current_context(void) {
  if (current_thread < 0 || current_thread >= thread_capacity)
    return 0;
  return threads[current_thread].context;
}

void sched_set_current_context(void *context) {
  if (current_thread < 0 || current_thread >= thread_capacity)
    return;
  threads[current_thread].context = context;
}

int sched_is_alive(int tid) {
  if (tid < 0 || tid >= thread_capacity)
    return 0;

  return threads[tid].state == THREAD_READY ||
         threads[tid].state == THREAD_RUNNING ||
         threads[tid].state == THREAD_BLOCKED;
}
