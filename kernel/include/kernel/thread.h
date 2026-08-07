#ifndef KERNEL_THREAD_H
#define KERNEL_THREAD_H

typedef void (*thread_entry_fn)(void);

void sched_init(void);
int sched_spawn(thread_entry_fn entry);
void sched_yield(void);

#endif
