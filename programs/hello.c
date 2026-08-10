#include "syscall.h"
__attribute__((section(".text._start"))) void _start(void) {
  const char msg[] = "Hello from userspace!\n";

  sys_write(msg, sizeof(msg) - 1);

  sys_exit(0);
}
