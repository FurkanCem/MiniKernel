#include "kernel/pipe.h"
#include "kernel/io.h"
#include "kernel/thread.h"

#define PIPE_MAX 8
#define PIPE_BUF_SIZE 1024

typedef struct {
  int in_use;
  unsigned char buf[PIPE_BUF_SIZE];
  unsigned int read_pos;
  unsigned int write_pos;
  unsigned int count;
  int readers;
  int writers;
} pipe_t;

static pipe_t pipes[PIPE_MAX];

static int valid(int idx) {
  return idx >= 0 && idx < PIPE_MAX && pipes[idx].in_use;
}

static void maybe_free(int idx) {
  if (pipes[idx].readers <= 0 && pipes[idx].writers <= 0)
    pipes[idx].in_use = 0;
}

int pipe_create(void) {
  for (int i = 0; i < PIPE_MAX; i++) {
    if (!pipes[i].in_use) {
      pipes[i].in_use = 1;
      pipes[i].read_pos = 0;
      pipes[i].write_pos = 0;
      pipes[i].count = 0;
      pipes[i].readers = 1;
      pipes[i].writers = 1;
      return i;
    }
  }
  return -1;
}

int pipe_add_reader(int idx) {
  if (!valid(idx))
    return -1;
  unsigned long long flags = irq_save();
  pipes[idx].readers++;
  irq_restore(flags);
  return 0;
}

int pipe_add_writer(int idx) {
  if (!valid(idx))
    return -1;
  unsigned long long flags = irq_save();
  pipes[idx].writers++;
  irq_restore(flags);
  return 0;
}

void pipe_close_reader(int idx) {
  if (idx < 0 || idx >= PIPE_MAX || !pipes[idx].in_use)
    return;
  unsigned long long flags = irq_save();
  if (pipes[idx].readers > 0)
    pipes[idx].readers--;
  maybe_free(idx);
  irq_restore(flags);
  sched_wakeup(&pipes[idx]);
}

void pipe_close_writer(int idx) {
  if (idx < 0 || idx >= PIPE_MAX || !pipes[idx].in_use)
    return;
  unsigned long long flags = irq_save();
  if (pipes[idx].writers > 0)
    pipes[idx].writers--;
  maybe_free(idx);
  irq_restore(flags);
  sched_wakeup(&pipes[idx]);
}

int pipe_read(int idx, void *buf, unsigned int len) {
  if (idx < 0 || idx >= PIPE_MAX)
    return -1;

  unsigned char *dst = (unsigned char *)buf;

  for (;;) {
    unsigned long long flags = irq_save();

    if (!pipes[idx].in_use) {
      irq_restore(flags);
      return -1;
    }

    if (pipes[idx].count > 0) {
      unsigned int n = pipes[idx].count < len ? pipes[idx].count : len;
      for (unsigned int i = 0; i < n; i++) {
        dst[i] = pipes[idx].buf[pipes[idx].read_pos];
        pipes[idx].read_pos = (pipes[idx].read_pos + 1) % PIPE_BUF_SIZE;
      }
      pipes[idx].count -= n;
      irq_restore(flags);
      sched_wakeup(&pipes[idx]);
      return (int)n;
    }

    if (pipes[idx].writers <= 0) {
      irq_restore(flags);
      return 0; /* EOF: nothing buffered, and nobody left to write more */
    }

    sched_sleep(&pipes[idx]);
    irq_restore(flags);
  }
}

int pipe_write(int idx, const void *buf, unsigned int len) {
  if (idx < 0 || idx >= PIPE_MAX)
    return -1;

  const unsigned char *src = (const unsigned char *)buf;

  for (;;) {
    unsigned long long flags = irq_save();

    if (!pipes[idx].in_use) {
      irq_restore(flags);
      return -1;
    }

    if (pipes[idx].readers <= 0) {
      irq_restore(flags);
      return -1; /* broken pipe: nobody left to ever read this */
    }

    unsigned int free_space = PIPE_BUF_SIZE - pipes[idx].count;
    if (free_space > 0) {
      unsigned int n = free_space < len ? free_space : len;
      for (unsigned int i = 0; i < n; i++) {
        pipes[idx].buf[pipes[idx].write_pos] = src[i];
        pipes[idx].write_pos = (pipes[idx].write_pos + 1) % PIPE_BUF_SIZE;
      }
      pipes[idx].count += n;
      irq_restore(flags);
      sched_wakeup(&pipes[idx]);
      return (int)n;
    }

    sched_sleep(&pipes[idx]);
    irq_restore(flags);
  }
}
