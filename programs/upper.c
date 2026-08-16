#include "syscall.h"

__attribute__((section(".text._start"))) void _start(void) {
  char c;
  long n;

  while ((n = sys_read(STDIN, &c, 1)) > 0) {
    if (c >= 'a' && c <= 'z')
      c = (char)(c - 'a' + 'A');
    sys_write(STDOUT, &c, 1);
  }

  sys_exit(0);
}
