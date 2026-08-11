#include "shell_internal.h"
#include "kernel/video.h"

int str_eq(const char *a, const char *b) {
  unsigned int i = 0;
  while (a[i] != '\0' && b[i] != '\0') {
    if (a[i] != b[i])
      return 0;
    i++;
  }
  return a[i] == '\0' && b[i] == '\0';
}

int str_starts_with(const char *str, const char *prefix) {
  unsigned int i = 0;
  while (prefix[i] != '\0') {
    if (str[i] != prefix[i])
      return 0;
    i++;
  }
  return 1;
}

void print_str(const char *str) {
  for (unsigned int i = 0; str[i] != '\0'; i++) {
    putchar_at_cursor(str[i]);
  }
}

void print_hex(unsigned long long value) {
  static const char digits[] = "0123456789ABCDEF";
  char buf[19];

  buf[0] = '0';
  buf[1] = 'x';
  for (int i = 0; i < 16; i++) {
    buf[2 + i] = digits[(value >> ((15 - i) * 4)) & 0xF];
  }
  buf[18] = '\0';

  print_str(buf);
}
