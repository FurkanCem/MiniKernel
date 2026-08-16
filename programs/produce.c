#include "syscall.h"

__attribute__((section(".text._start"))) void _start(void) {
  const char *lines[] = {"alpha\n", "beta\n", "gamma\n"};

  for (int i = 0; i < 3; i++) {
    unsigned long len = 0;
    while (lines[i][len] != '\0')
      len++;
    sys_write(STDOUT, lines[i], len);
  }

  sys_exit(0);
}
