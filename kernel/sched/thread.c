#include "kernel/thread.h"
#include "kernel/heap.h"

#define MAX_THREADS 8
#define THREAD_STACK_SIZE 4000

typedef enum {
    THREAD_UNUSED,
    THREAD_READY,
    THREAD_RUNNING
} thread_state_t;

typedef struct {
    unsigned long long rsp;
    thread_state_t state;
    void *stack_base;
} thread_t;

static thread_t threads[MAX_THREADS];
static int current_thread = -1;

extern void switch_context(unsigned long long *old_rsp, unsigned long long *new_rsp);

void sched_init(void){
    for (int i = 0; i < MAX_THREADS; i++){
        threads[i].state = THREAD_UNUSED;
        threads[i].stack_base = 0;
        threads[i].rsp = 0;
    }

    threads[0].state = THREAD_RUNNING;
    current_thread = 0;
}

int sched_spawn(thread_entry_fn entry){
    int slot = -1;
    for (int i = 0; i < MAX_THREADS; i++){
        if (threads[i].state == THREAD_UNUSED){
            slot = i;
            break;
        }
    }
    if (slot == -1)
        return -1;

    void *stack = kmalloc(THREAD_STACK_SIZE);
    if (stack == 0)
        return -1;

    unsigned long long *sp = (unsigned long long *)((unsigned char *)stack + THREAD_STACK_SIZE);

    sp -= 1; *sp = (unsigned long long)entry;
    sp -= 1; *sp = 0;
    sp -= 1; *sp = 0;
    sp -= 1; *sp = 0;
    sp -= 1; *sp = 0;
    sp -= 1; *sp = 0;
    sp -= 1; *sp = 0;

    threads[slot].rsp = (unsigned long long)sp;
    threads[slot].state = THREAD_READY;
    threads[slot].stack_base = stack;

    return slot;
}

void sched_yield(void){
    if (current_thread == -1)
        return;

    int next = current_thread;
    for (int i = 0; i < MAX_THREADS; i++){
        next = (next + 1) % MAX_THREADS;
        if (threads[next].state == THREAD_READY || threads[next].state == THREAD_RUNNING)
            break;
    }

    if (next == current_thread)
        return;

    threads[current_thread].state = THREAD_READY;
    threads[next].state = THREAD_RUNNING;

    int prev = current_thread;
    current_thread = next;

    switch_context(&threads[prev].rsp, &threads[next].rsp);
}
