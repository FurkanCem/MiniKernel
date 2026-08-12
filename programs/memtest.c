#include "syscall.h"

static unsigned long str_len(const char *s) {
  unsigned long n = 0;
  while (s[n] != '\0')
    n++;
  return n;
}

static void print(const char *s) { sys_write(STDOUT, s, str_len(s)); }

__attribute__((section(".text._start"))) void _start(void) {
  const char msg[] = "Hello, memfs!\n";

  print("[+] creating 'greeting' and writing to it\n");

  long fd = sys_open("greeting", O_CREATE);
  if (fd < 0) {
    print("[-] sys_open (create) failed\n");
    sys_exit(1);
  }

  sys_write((int)fd, msg, sizeof(msg) - 1);
  sys_close((int)fd);

  print("[+] reopening 'greeting' and reading it back\n");

  fd = sys_open("greeting", 0);
  if (fd < 0) {
    print("[-] sys_open (read) failed\n");
    sys_exit(1);
  }

  char buf[64];
  long n = sys_read((int)fd, buf, sizeof(buf) - 1);
  sys_close((int)fd);

  if (n < 0) {
    print("[-] sys_read failed\n");
    sys_exit(1);
  }

  buf[n] = '\0';

  print("[+] read back: ");
  print(buf);

  sys_exit(0);
}
