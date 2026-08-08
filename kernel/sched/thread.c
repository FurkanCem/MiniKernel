#include "kernel/thread.h"
#include "kernel/io.h"
#include "kernel/pmm.h"
#include "kernel/vmm.h"

#define MAX_THREADS 8
#define THREAD_STACK_PAGES 2 /* 8KB usable stack per thread */
#define THREAD_STACK_SIZE (THREAD_STACK_PAGES * PMM_FRAME_SIZE)

typedef enum {
  THREAD_UNUSED,
  THREAD_READY,
  THREAD_RUNNING,
  THREAD_ZOMBIE
} thread_state_t;

/* Quantum length in timer ticks for each priority - not "who runs first"
 * (selection is still plain round robin), but "how long they keep the CPU
 * once picked". At 100Hz a HIGH-priority thread gets a ~80ms slice before
 * being forced to give way; a LOW one gets ~20ms. */
static const int quantum_ticks[THREAD_PRIO_COUNT] = {
    [THREAD_PRIO_LOW] = 2,
    [THREAD_PRIO_NORMAL] = 4,
    [THREAD_PRIO_HIGH] = 8,
};

typedef struct {
  unsigned long long rsp;
  thread_state_t state;
  unsigned long long stack_low;  /* lowest address of the usable stack */
  unsigned long long guard_page; /* the unmapped page just below it */
  thread_priority_t priority;
  int ticks_remaining; /* counts down each sched_tick(); yields at 0 */
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
    /* Give the frames back to the allocator and undo the guard so this
     * physical memory is ordinary usable RAM again, not a permanent hole
     * in the address space every time a thread exits. */
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

void sched_init(void) {
  for (int i = 0; i < MAX_THREADS; i++) {
    threads[i].state = THREAD_UNUSED;
    threads[i].stack_low = 0;
    threads[i].guard_page = 0;
    threads[i].rsp = 0;
    threads[i].priority = THREAD_PRIO_NORMAL;
    threads[i].ticks_remaining = quantum_ticks[THREAD_PRIO_NORMAL];
  }

  threads[0].state = THREAD_RUNNING;
  current_thread = 0;
  zombie_slot = -1;
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

  /* One extra frame on the low side of the stack, deliberately left
   * unmapped: [[GUARD][page 0][page 1]...] with the guard immediately
   * below the lowest byte the stack is allowed to touch. An overflow now
   * takes a page fault at a precise, logged address instead of silently
   * corrupting whatever the heap put next door. */
  unsigned long long region = pmm_alloc_contiguous(THREAD_STACK_PAGES + 1);
  if (region == 0)
    return -1;

  unsigned long long guard_page = region;
  unsigned long long stack_low = region + PMM_FRAME_SIZE;

  /* The whole region came back from vmm_init()'s blanket identity map
   * already present - explicitly unmap just the guard page. */
  vmm_guard_page(guard_page);
  if (vmm_is_mapped(guard_page)) {
    /* Splitting the 2MB region failed (out of memory for the new page
     * table) - back out the frames rather than run without a guard. */
    for (unsigned long long i = 0; i < THREAD_STACK_PAGES + 1; i++)
      pmm_free_frame(region + i * PMM_FRAME_SIZE);
    return -1;
  }

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

  return slot;
}

int sched_spawn(thread_entry_fn entry) {
  return sched_spawn_prio(entry, THREAD_PRIO_NORMAL);
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

  switch_context(&threads[prev].rsp, &threads[next].rsp);

  irq_restore(flags);
}

void sched_tick(void) {
  if (current_thread == -1)
    return;

  if (--threads[current_thread].ticks_remaining > 0)
    return;

  sched_yield(); /* quantum expired - let someone else run */
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
    irq_restore(flags); /* re-enable interrupts or hlt spins forever */
    for (;;)
      __asm__ volatile("hlt");
  }

  threads[next].state = THREAD_RUNNING;
  current_thread = next;

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
