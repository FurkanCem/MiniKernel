#include "syscall.h"

#define LINE_MAX 120

static unsigned long str_len(const char *s) {
  unsigned long n = 0;
  while (s[n] != '\0')
    n++;
  return n;
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

static void print(const char *s) { sys_write(STDOUT, s, str_len(s)); }

static unsigned long read_line(char *buf, unsigned long max) {
  unsigned long len = 0;

  for (;;) {
    char c;
    sys_read(STDIN, &c, 1);

    if (c == '\n') {
      sys_write(STDOUT, &c, 1);
      break;
    }

    if (c == '\b') {
      if (len > 0) {
        len--;
        sys_write(STDOUT, &c, 1);
      }
      continue;
    }

    if (len < max - 1) {
      buf[len++] = c;
      sys_write(STDOUT, &c, 1);
    }
  }

  buf[len] = '\0';
  return len;
}

__attribute__((section(".text._start"))) void _start(void) {
  char line[LINE_MAX];

  print("MiniKernel userspace shell\n");

  for (;;) {
    print("sh> ");

    unsigned long len = read_line(line, LINE_MAX);

    if (len == 0)
      continue;

    if (str_eq(line, "exit")) {
      sys_exit(0);
    }

    long pid = sys_spawn(line, len);
    if (pid < 0) {
      print("spawn failed: ");
      print(line);
      print("\n");
      continue;
    }

    sys_wait(pid);
  }
}
