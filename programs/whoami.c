#include "syscall.h"

static void print(const char *s) {
  unsigned long len = 0;
  while (s[len] != '\0')
    len++;
  sys_write(STDOUT, s, len);
}

static void print_unsigned(unsigned long value) {
  char digits[sizeof(value) * 3];
  unsigned long count = 0;

  do {
    digits[count++] = (char)('0' + value % 10);
    value /= 10;
  } while (value != 0);

  while (count > 0) {
    count--;
    sys_write(STDOUT, &digits[count], 1);
  }
}

__attribute__((section(".text._start"))) void _start(void) {
  long tid = sys_whoami();

  print("thread id: ");
  if (tid < 0)
    print("unavailable");
  else
    print_unsigned((unsigned long)tid);
  print("\nexecution mode: user (ring 3)\n");

  sys_exit(0);
}
