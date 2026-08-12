#include "shell_internal.h"
#include "kernel/thread.h"
#include "kernel/video.h"

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
  sched_exit();
}

static void demo_thread_b(void) {
  for (int i = 0; i < 20000000; i++) {
    if (i % 200000 == 0)
      putchar_at_cursor('B');
  }
  sched_exit();
}

void cmd_threadtest(void) {
  print_str("\nspawning two threads that print A/B and yield:\n");

  int tid_a = sched_spawn(demo_thread_a);
  int tid_b = sched_spawn(demo_thread_b);

  sched_wait(tid_a);
  sched_wait(tid_b);

  print_str("\nboth threads finished");
}

void cmd_largethreadtest(void) {
  int size = 8;
  int tids[8];

  for (int i = 0; i < size; i++) {
    tids[i] = sched_spawn(demo_thread);
  }

  for (int i = 0; i < size; i++) {
    sched_wait(tids[i]);
  }

  print_str("\nthreads finished");
}
