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

#define FD_KIND_UNUSED 0
#define FD_KIND_CONSOLE 1
#define FD_KIND_MEMFS 2

#define THREAD_MAX_FDS 8

typedef struct {
  int kind;
  int handle;
  unsigned int cursor;
} fd_slot_t;

void sched_init(void);
int sched_spawn(thread_entry_fn entry);
int sched_spawn_with_context(thread_entry_fn entry, void *context);
int sched_spawn_prio(thread_entry_fn entry, thread_priority_t priority);
void sched_set_priority(int tid, thread_priority_t priority);
void sched_set_address_space(int tid, vmm_address_space_t space);

void sched_yield(void);
void sched_tick(void);

void sched_sleep(const void *channel);
void sched_wakeup(const void *channel);
void sched_wait(int tid);

int sched_fd_open(int kind, int handle);
int sched_fd_close(int fd);
fd_slot_t *sched_fd_get(int fd);

void sched_exit(void);
void sched_exit_status(int status);
void sched_reap_thread(int tid);
int sched_is_alive(int);
int sched_current_tid(void);
void *sched_current_context(void);
void sched_set_current_context(void *context);
#endif
