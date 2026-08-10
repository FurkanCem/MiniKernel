#ifndef USER_SYSCALL_H
#define USER_SYSCALL_H

#define SYS_WRITE_HELLO 1
#define SYS_EXIT 2
#define SYS_WRITE 3
#define SYS_READ 4

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

static inline long sys_write(const char *buf, unsigned long len) {
  return syscall2(SYS_WRITE, (unsigned long)buf, len);
}

static inline void sys_exit(int status) {
  syscall1(SYS_EXIT, (unsigned long)status);

  __builtin_unreachable();
}

#endif
