#include "syscall.h"

#define PAGE_SIZE 4096UL

static void print(const char *s) {
  unsigned long len = 0;
  while (s[len] != '\0')
    len++;
  sys_write(STDOUT, s, len);
}

static void print_unsigned(unsigned long value) {
  char digits[3 * sizeof(value)];
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
  unsigned long total_frames = 0;
  unsigned long free_frames = 0;

  if (sys_meminfo(&total_frames, &free_frames) < 0) {
    print("meminfo: syscall failed\n");
    sys_exit(1);
  }

  print("memory (physical frame allocator)\n");
  print("total frames: ");
  print_unsigned(total_frames);
  print("\nfree frames:  ");
  print_unsigned(free_frames);
  print("\nused frames:  ");
  print_unsigned(total_frames - free_frames);
  print("\nfree bytes:   ");
  print_unsigned(free_frames * PAGE_SIZE);
  print("\n");

  sys_exit(0);
}
