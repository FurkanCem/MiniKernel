#include "kernel/fd.h"
#include "kernel/keyboard.h"
#include "kernel/memfs.h"
#include "kernel/pipe.h"
#include "kernel/thread.h"
#include "kernel/ufs.h"
#include "kernel/video.h"

int fd_open(const char *name, unsigned long long flags) {
  if (flags & FD_PERSIST) {
    int handle = ufs_find(name);
    if (handle < 0) {
      if (!(flags & FD_CREATE))
        return -1;

      unsigned int me = sched_get_uid(sched_current_tid());
      handle = ufs_create(name, me);
      if (handle < 0)
        return -1;
    } else {
      unsigned int me = sched_get_uid(sched_current_tid());
      int is_owner = (me == ufs_owner(handle));

      if (!is_owner && !(ufs_perm(handle) & UFS_PERM_OTHER_READ))
        return -1;

      if (flags & FD_TRUNC) {
        if (!is_owner && !(ufs_perm(handle) & UFS_PERM_OTHER_WRITE))
          return -1;
        ufs_truncate(handle, 0);
      }
    }

    return sched_fd_open(FD_KIND_UFS, handle);
  }

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

static void release_slot(fd_slot_t *slot) {
  if (slot->kind == FD_KIND_PIPE_READ) {
    pipe_close_reader(slot->handle);
  } else if (slot->kind == FD_KIND_PIPE_WRITE) {
    pipe_close_writer(slot->handle);
  }
}

void fd_cleanup_thread(int tid) {
  for (int fd = 0; fd < THREAD_MAX_FDS; fd++) {
    fd_slot_t *slot = sched_fd_get_for(tid, fd);
    if (slot != 0)
      release_slot(slot);
  }
}

int fd_close(int fd) {
  fd_slot_t *slot = sched_fd_get(fd);
  if (slot == 0)
    return -1;

  release_slot(slot);
  return sched_fd_close(fd);
}

long fd_read(int fd, void *buf, unsigned long long len) {
  fd_slot_t *slot = sched_fd_get(fd);
  if (slot == 0)
    return -1;

  if (slot->kind == FD_KIND_CONSOLE) {
    char *dst = (char *)buf;
    for (unsigned long long i = 0; i < len; i++) {
      dst[i] = kbd_getchar();
    }
    return (long)len;
  }

  if (slot->kind == FD_KIND_PIPE_READ)
    return pipe_read(slot->handle, buf, (unsigned int)len);

  int n;
  if (slot->kind == FD_KIND_UFS) {
    unsigned int me = sched_get_uid(sched_current_tid());
    if (me != ufs_owner(slot->handle) &&
        !(ufs_perm(slot->handle) & UFS_PERM_OTHER_READ))
      return -1;
    n = ufs_read_at(slot->handle, slot->cursor, buf, (unsigned int)len);
  } else if (slot->kind == FD_KIND_MEMFS) {
    n = memfs_read_at(slot->handle, slot->cursor, buf, (unsigned int)len);
  } else {
    return -1;
  }

  if (n > 0)
    slot->cursor += (unsigned int)n;

  return n;
}

long fd_write(int fd, const void *buf, unsigned long long len) {
  fd_slot_t *slot = sched_fd_get(fd);
  if (slot == 0)
    return -1;

  if (slot->kind == FD_KIND_CONSOLE) {
    const char *src = (const char *)buf;
    for (unsigned long long i = 0; i < len; i++) {
      putchar_at_cursor(src[i]);
    }
    return (long)len;
  }

  if (slot->kind == FD_KIND_PIPE_WRITE)
    return pipe_write(slot->handle, buf, (unsigned int)len);

  int n;
  if (slot->kind == FD_KIND_UFS) {
    unsigned int me = sched_get_uid(sched_current_tid());
    if (me != ufs_owner(slot->handle) &&
        !(ufs_perm(slot->handle) & UFS_PERM_OTHER_WRITE))
      return -1;
    n = ufs_write_at(slot->handle, slot->cursor, buf, (unsigned int)len);
  } else if (slot->kind == FD_KIND_MEMFS) {
    n = memfs_write_at(slot->handle, slot->cursor, buf, (unsigned int)len);
  } else {
    return -1;
  }

  if (n > 0)
    slot->cursor += (unsigned int)n;

  return n;
}
