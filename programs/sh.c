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

static int str_starts_with(const char *s, const char *prefix) {
  unsigned long i = 0;
  while (prefix[i] != '\0') {
    if (s[i] != prefix[i])
      return 0;
    i++;
  }
  return 1;
}

static void print_ulong(unsigned long value) {
  char digits[20];
  int n = 0;

  if (value == 0) {
    sys_write(STDOUT, "0", 1);
    return;
  }

  while (value > 0) {
    digits[n++] = (char)('0' + (value % 10));
    value /= 10;
  }

  while (n > 0) {
    n--;
    sys_write(STDOUT, &digits[n], 1);
  }
}

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

static void cmd_ls(void) {
  char name[32];

  for (unsigned long index = 0;; index++) {
    if (sys_list(index, name, sizeof(name)) < 0)
      break;

    print(name);
    print("\n");
  }
}

static void cmd_write(const char *name) {
  if (name[0] == '\0') {
    print("usage: write <name>\n");
    return;
  }

  long fd = sys_open(name, O_CREATE | O_PERSIST | O_TRUNC);
  if (fd < 0) {
    print("write: could not open '");
    print(name);
    print("'\n");
    return;
  }

  print("content> ");
  char line[LINE_MAX];
  unsigned long len = read_line(line, LINE_MAX);

  sys_write((int)fd, line, len);
  sys_close((int)fd);

  print("saved ");
  print(name);
  print(" (");
  print_ulong(len);
  print(" bytes, persists across reboot)\n");
}

static void cmd_cat(const char *name) {
  if (name[0] == '\0') {
    print("usage: cat <name>\n");
    return;
  }

  long fd = sys_open(name, O_PERSIST);
  if (fd < 0) {
    print("cat: no such file: ");
    print(name);
    print("\n");
    return;
  }

  char buf[128];
  long n;
  while ((n = sys_read((int)fd, buf, sizeof(buf))) > 0) {
    sys_write(STDOUT, buf, (unsigned long)n);
  }
  print("\n");

  sys_close((int)fd);
}

static void cmd_rm(const char *name) {
  if (name[0] == '\0') {
    print("usage: rm <name>\n");
    return;
  }

  if (sys_remove(name) < 0) {
    print("rm: no such file: ");
    print(name);
    print("\n");
    return;
  }

  print("removed ");
  print(name);
  print("\n");
}

__attribute__((section(".text._start"))) void _start(void) {
  char line[LINE_MAX];

  print("MiniKernel userspace shell\n");
  print("type a program name to run it, 'ls' to list files, 'exit' to quit\n");
  print("'write <name>' / 'cat <name>' / 'rm <name>' for persistent files\n");

  for (;;) {
    print("sh> ");

    unsigned long len = read_line(line, LINE_MAX);

    if (len == 0)
      continue;

    if (str_eq(line, "exit")) {
      sys_exit(0);
    }

    if (str_eq(line, "ls")) {
      cmd_ls();
      continue;
    }

    if (str_starts_with(line, "write ")) {
      cmd_write(line + 6);
      continue;
    }

    if (str_starts_with(line, "cat ")) {
      cmd_cat(line + 4);
      continue;
    }

    if (str_starts_with(line, "rm ")) {
      cmd_rm(line + 3);
      continue;
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
