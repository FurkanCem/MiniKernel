#include "shell_internal.h"
#include "kernel/thread.h"
#include "kernel/video.h"

static volatile int demo_a_done = 0;
static volatile int demo_b_done = 0;

static void demo_thread(void) {
  int id = sched_current_tid();

  for (int i = 0; i < 20000000; i++) {
    if (i % (400 * id) == 0)
      putchar_at_cursor('A' + id);
  }

  sched_exit();
}

static void demo_thread_a(void) {
  for (int i = 0; i < 20000000; i++) {
    if (i % 200000 == 0)
      putchar_at_cursor('A');
  }
  demo_a_done = 1;
  sched_exit();
}

static void demo_thread_b(void) {
  for (int i = 0; i < 20000000; i++) {
    if (i % 200000 == 0)
      putchar_at_cursor('B');
  }
  demo_b_done = 1;
  sched_exit();
}

void cmd_threadtest(void) {
  demo_a_done = 0;
  demo_b_done = 0;

  print_str("\nspawning two threads that print A/B and yield:\n");

  sched_spawn(demo_thread_a);
  sched_spawn(demo_thread_b);

  while (!demo_a_done || !demo_b_done) {
    sched_yield();
  }

  print_str("\nboth threads finished");
}

void cmd_largethreadtest(void) {
  int size = 8;
  int tids[8];

  for (int i = 0; i < size; i++) {
    tids[i] = sched_spawn(demo_thread);
  }

  for (;;) {
    int finished = 1;

    for (int i = 0; i < size; i++) {
      if (sched_is_alive(tids[i])) {
        finished = 0;
        break;
      }
    }

    if (finished)
      break;

    sched_yield();
  }

  print_str("\nthreads finished");
}
