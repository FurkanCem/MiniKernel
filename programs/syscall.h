#ifndef USER_SYSCALL_H
#define USER_SYSCALL_H

#define SYS_WRITE_HELLO 1
#define SYS_EXIT 2
#define SYS_WRITE 3
#define SYS_READ 4
#define SYS_SPAWN 5
#define SYS_WAIT 6
#define SYS_OPEN 7
#define SYS_CLOSE 8
#define SYS_LIST 9
#define SYS_WHOAMI 10
#define SYS_MEMINFO 11
#define SYS_REMOVE 12
#define SYS_CLEAR 13
#define SYS_SET_CURSOR 14
#define SYS_DRAW_ROW 15
#define SYS_SCREEN_INFO 16
#define SYS_KILL 17
#define SYS_PIPE 18
#define SYS_SPAWN_REDIRECT 19
#define SYS_GETUID 20
#define SYS_CHMOD 21
#define SYS_LOGIN 22
#define SYS_ADDUSER 23
#define SYS_USERNAME 24

#define UFS_PERM_OTHER_READ 0x1
#define UFS_PERM_OTHER_WRITE 0x2

#define SIGTERM 15
#define SIGKILL 9

#define STDIN 0
#define STDOUT 1

#define O_CREATE 1UL
/* Opens the file on the disk-persistent filesystem instead of the
 * default in-RAM one, so its contents survive a reboot. */
#define O_PERSIST 2UL
#define O_TRUNC 4UL

static inline long syscall0(long number) {
  long ret;

  __asm__ volatile("int $0x80"
                   : "=a"(ret)
                   : "a"(number)
                   : "rcx", "r11", "memory");

  return ret;
}

static inline long syscall1(long number, unsigned long arg1) {
  long ret;

  __asm__ volatile("int $0x80"
                   : "=a"(ret)
                   : "a"(number), "D"(arg1)
                   : "rcx", "r11", "memory");

  return ret;
}

static inline long syscall2(long number, unsigned long arg1,
                            unsigned long arg2) {
  long ret;

  __asm__ volatile("int $0x80"
                   : "=a"(ret)
                   : "a"(number), "S"(arg1), "d"(arg2)
                   : "rcx", "r11", "memory");

  return ret;
}

static inline long syscall3(long number, unsigned long arg1,
                            unsigned long arg2, unsigned long arg3) {
  long ret;

  __asm__ volatile("int $0x80"
                   : "=a"(ret)
                   : "a"(number), "D"(arg1), "S"(arg2), "d"(arg3)
                   : "rcx", "r11", "memory");

  return ret;
}

static inline long sys_write(int fd, const char *buf, unsigned long len) {
  return syscall3(SYS_WRITE, (unsigned long)fd, (unsigned long)buf, len);
}

static inline long sys_read(int fd, char *buf, unsigned long len) {
  return syscall3(SYS_READ, (unsigned long)fd, (unsigned long)buf, len);
}

static inline long sys_open(const char *name, unsigned long flags) {
  return syscall2(SYS_OPEN, (unsigned long)name, flags);
}

static inline long sys_close(int fd) {
  return syscall1(SYS_CLOSE, (unsigned long)fd);
}

static inline long sys_remove(const char *name) {
  return syscall1(SYS_REMOVE, (unsigned long)name);
}

static inline long sys_clear(void) { return syscall0(SYS_CLEAR); }

static inline long sys_set_cursor(unsigned long row, unsigned long col) {
  return syscall2(SYS_SET_CURSOR, row, col);
}

/* Redraws an entire screen row (space-padded) from `buf`. `len` may be
 * shorter than the screen width; it must not exceed 256 bytes. */
static inline long sys_draw_row(unsigned long row, const char *buf,
                                unsigned long len) {
  return syscall3(SYS_DRAW_ROW, row, (unsigned long)buf, len);
}

static inline long sys_screen_info(unsigned long *cols, unsigned long *rows) {
  return syscall2(SYS_SCREEN_INFO, (unsigned long)cols, (unsigned long)rows);
}

static inline long sys_kill(long pid, long sig) {
  return syscall2(SYS_KILL, (unsigned long)pid, (unsigned long)sig);
}

/* fds_out[0] receives the read end, fds_out[1] the write end. */
static inline long sys_pipe(long fds_out[2]) {
  return syscall1(SYS_PIPE, (unsigned long)fds_out);
}

/* Like sys_spawn, but redirects the new process's stdin/stdout to
 * pipe fds you already have open (from sys_pipe) instead of the
 * console. Pass -1 for either one to leave it on the console. */
static inline long sys_spawn_redirect(const char *cmdline, unsigned long len,
                                       long stdin_fd, long stdout_fd) {
  unsigned long packed = ((unsigned long)stdin_fd & 0xFFFFUL) |
                          (((unsigned long)stdout_fd & 0xFFFFUL) << 16);
  return syscall3(SYS_SPAWN_REDIRECT, (unsigned long)cmdline, len, packed);
}

static inline long sys_getuid(void) { return syscall0(SYS_GETUID); }

/* Sets who besides the owner can read/write a UFS file (UFS_PERM_*
 * bits, OR'd together - 0 means private). Only the file's owner can
 * do this; fails otherwise. */
static inline long sys_chmod(const char *name, unsigned long perm) {
  return syscall2(SYS_CHMOD, (unsigned long)name, perm);
}

static inline long sys_login(const char *username, const char *password) {
  return syscall2(SYS_LOGIN, (unsigned long)username, (unsigned long)password);
}

/* Root-only (the kernel checks the caller's uid, not this wrapper). */
static inline long sys_adduser(const char *username, const char *password,
                                unsigned long uid) {
  return syscall3(SYS_ADDUSER, (unsigned long)username, (unsigned long)password,
                   uid);
}

static inline long sys_username(unsigned long uid, char *out,
                                 unsigned long out_len) {
  return syscall3(SYS_USERNAME, uid, (unsigned long)out, out_len);
}

static inline long sys_list(unsigned long index, char *buf,
                             unsigned long buf_len) {
  return syscall3(SYS_LIST, index, (unsigned long)buf, buf_len);
}

static inline long sys_whoami(void) { return syscall0(SYS_WHOAMI); }

static inline long sys_meminfo(unsigned long *total_frames,
                               unsigned long *free_frames) {
  return syscall2(SYS_MEMINFO, (unsigned long)total_frames,
                  (unsigned long)free_frames);
}

static inline long sys_spawn(const char *cmdline, unsigned long len) {
  return syscall2(SYS_SPAWN, (unsigned long)cmdline, len);
}

static inline long sys_wait(long pid) { return syscall1(SYS_WAIT, (unsigned long)pid); }

static inline void sys_exit(int status) {
  syscall1(SYS_EXIT, (unsigned long)status);

  __builtin_unreachable();
}

#endif
