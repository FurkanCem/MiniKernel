#ifndef KERNEL_THREAD_H
#define KERNEL_THREAD_H

#include "kernel/vmm.h"

typedef void (*thread_entry_fn)(void);

typedef enum {
  THREAD_PRIO_LOW = 0,
  THREAD_PRIO_NORMAL,
  THREAD_PRIO_HIGH,
  THREAD_PRIO_COUNT
} thread_priority_t;

void sched_init(void);
int sched_spawn(thread_entry_fn entry);
int sched_spawn_prio(thread_entry_fn entry, thread_priority_t priority);
void sched_set_priority(int tid, thread_priority_t priority);
void sched_set_address_space(int tid, vmm_address_space_t space);

void sched_yield(void);
void sched_tick(void);

void sched_exit(void);
int sched_is_alive(int);
int sched_current_tid(void);
#endif
