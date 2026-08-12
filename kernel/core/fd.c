#include "kernel/fd.h"
#include "kernel/keyboard.h"
#include "kernel/memfs.h"
#include "kernel/thread.h"
#include "kernel/video.h"

#define FD_STDIN 0
#define FD_STDOUT 1

int fd_open(const char *name, unsigned long long flags) {
  int handle = memfs_find(name);
  if (handle < 0) {
    if (!(flags & FD_CREATE))
      return -1;

    handle = memfs_create(name);
    if (handle < 0)
      return -1;
  }

  return sched_fd_open(FD_KIND_MEMFS, handle);
}

int fd_close(int fd) {
  if (fd == FD_STDIN || fd == FD_STDOUT)
    return 0;

  return sched_fd_close(fd);
}

long fd_read(int fd, void *buf, unsigned long long len) {
  if (fd == FD_STDIN) {
    char *dst = (char *)buf;
    for (unsigned long long i = 0; i < len; i++) {
      dst[i] = kbd_getchar();
    }
    return (long)len;
  }

  fd_slot_t *slot = sched_fd_get(fd);
  if (slot == 0 || slot->kind != FD_KIND_MEMFS)
    return -1;

  int n = memfs_read_at(slot->handle, slot->cursor, buf, (unsigned int)len);
  if (n > 0)
    slot->cursor += (unsigned int)n;

  return n;
}

long fd_write(int fd, const void *buf, unsigned long long len) {
  if (fd == FD_STDOUT) {
    const char *src = (const char *)buf;
    for (unsigned long long i = 0; i < len; i++) {
      putchar_at_cursor(src[i]);
    }
    return (long)len;
  }

  fd_slot_t *slot = sched_fd_get(fd);
  if (slot == 0 || slot->kind != FD_KIND_MEMFS)
    return -1;

  int n = memfs_write_at(slot->handle, slot->cursor, buf, (unsigned int)len);
  if (n > 0)
    slot->cursor += (unsigned int)n;

  return n;
}
