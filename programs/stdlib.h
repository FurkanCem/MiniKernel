#ifndef STDLIB
#define STDLIB

#include "syscall.h"
typedef unsigned long long u64;
typedef unsigned int u32;

static u64 strlen(const char *str) {
  u64 len = 0;
  while (str[len] != '\0') {
    len++;
  }
  return len;
}

static int str_eq(const char *a, const char *b) {
  unsigned long i = 0;
  while (a[i] != '\0' && b[i] != '\0') {
    if (a[i] != b[i])
      return 0;
    i++;
  }
  return a[i] == '\0' && b[i] == '\0';
}

static void print_char(char c) { sys_write(STDOUT, &c, 1); }

static void print(const char *str) { sys_write(STDOUT, str, strlen(str)); }

static void println(const char *str) {
  print(str);
  print("\n");
}

static void print_u64(u64 value) {
  char buffer[32];
  int i = 0;

  if (value == 0) {
    print_char('0');
    return;
  }

  while (value > 0) {
    buffer[i++] = '0' + (value % 10);
    value /= 10;
  }

  while (i > 0)
    print_char(buffer[--i]);
}

static void print_unsigned(u64 value) { print_u64(value); }

static void print_hex(u64 value) {
  static const char hex[] = "0123456789abcdef";
  char buffer[18];
  int i;

  buffer[0] = '0';
  buffer[1] = 'x';

  for (i = 0; i < 16; i++) {
    int shift = (15 - i) * 4;
    buffer[2 + i] = hex[(value >> shift) & 0xf];
  }

  sys_write(STDOUT, buffer, sizeof(buffer));
}

#endif
