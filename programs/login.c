#include "syscall.h"

/* A minimal getty/login: prompts for credentials, authenticates via
 * sys_login() (which only changes this process's own uid if the
 * password actually matches - see kernel/core/syscall.c), then spawns
 * a shell that inherits the now-authenticated uid. Loops back to the
 * prompt when that shell exits, so logging out returns you here
 * rather than dropping you back to the kernel shell.
 *
 * Note `run sh` from the kernel shell still exists as a direct,
 * unauthenticated root shell - that's the physical-console/maintenance
 * bypass (whoever can type at the kernel shell already has full
 * access), while `run login` is the real gated entry point.
 */

static void print(const char *s) {
  unsigned long len = 0;
  while (s[len] != '\0')
    len++;
  sys_write(STDOUT, s, len);
}

static long read_line_echo(char *buf, unsigned long max_len) {
  unsigned long len = 0;
  while (1) {
    char c;
    sys_read(STDIN, &c, 1);
    if (c == '\n') {
      buf[len] = '\0';
      sys_write(STDOUT, "\n", 1);
      return (long)len;
    }
    if (c == '\b') {
      if (len > 0) {
        len--;
        sys_write(STDOUT, "\b \b", 3);
      }
      continue;
    }
    if (len + 1 < max_len) {
      buf[len++] = c;
      sys_write(STDOUT, &c, 1);
    }
  }
}

/* Same as read_line_echo, but shows '*' instead of the real character
 * typed, so a password isn't visible on screen. */
static long read_line_masked(char *buf, unsigned long max_len) {
  unsigned long len = 0;
  while (1) {
    char c;
    sys_read(STDIN, &c, 1);
    if (c == '\n') {
      buf[len] = '\0';
      sys_write(STDOUT, "\n", 1);
      return (long)len;
    }
    if (c == '\b') {
      if (len > 0) {
        len--;
        sys_write(STDOUT, "\b \b", 3);
      }
      continue;
    }
    if (len + 1 < max_len) {
      buf[len++] = c;
      sys_write(STDOUT, "*", 1);
    }
  }
}

__attribute__((section(".text._start"))) void _start(void) {
  char username[32];
  char password[64];

  for (;;) {
    print("\nMiniKernel login\n");
    print("username: ");
    read_line_echo(username, sizeof(username));
    print("password: ");
    read_line_masked(password, sizeof(password));

    if (sys_login(username, password) < 0) {
      print("login incorrect\n");
      continue;
    }

    print("welcome, ");
    print(username);
    print("\n");

    long pid = sys_spawn("sh", 2);
    if (pid < 0) {
      print("login: could not start shell\n");
      continue;
    }
    sys_wait(pid);
    /* Shell exited (e.g. `exit`) - loop back to the login prompt
     * rather than falling through to anything else. */
  }
}
