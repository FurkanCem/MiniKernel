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

/* Persists a single line of text under `name` on the disk-backed
 * filesystem, replacing any previous contents. This is a stopgap for
 * testing persistence until a real multi-line text editor exists. */
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

static void cmd_chmod(const char *args) {
  char name[32];
  unsigned long i = 0;
  while (args[i] != '\0' && args[i] != ' ' && i < sizeof(name) - 1) {
    name[i] = args[i];
    i++;
  }
  name[i] = '\0';

  const char *mode = args + i;
  while (*mode == ' ')
    mode++;

  unsigned long perm;
  if (str_eq(mode, "private")) {
    perm = 0;
  } else if (str_eq(mode, "readonly")) {
    perm = UFS_PERM_OTHER_READ;
  } else if (str_eq(mode, "public")) {
    perm = UFS_PERM_OTHER_READ | UFS_PERM_OTHER_WRITE;
  } else {
    print("usage: chmod <name> private|readonly|public\n");
    return;
  }

  if (name[0] == '\0' || sys_chmod(name, perm) < 0) {
    print("chmod: failed (not found, or you don't own it)\n");
    return;
  }

  print("chmod: ");
  print(name);
  print(" is now ");
  print(mode);
  print("\n");
}

/* usage: useradd <username> <password> <uid> - root only (the kernel
 * enforces this; a non-root caller just gets sys_adduser() failing). */
static void cmd_useradd(const char *args) {
  char username[20];
  unsigned long i = 0;
  while (args[i] != '\0' && args[i] != ' ' && i < sizeof(username) - 1) {
    username[i] = args[i];
    i++;
  }
  username[i] = '\0';

  const char *p = args + i;
  while (*p == ' ')
    p++;

  char password[32];
  unsigned long j = 0;
  while (*p != '\0' && *p != ' ' && j < sizeof(password) - 1) {
    password[j++] = *p++;
  }
  password[j] = '\0';

  while (*p == ' ')
    p++;

  long uid = 0;
  int have_uid = 0;
  while (*p >= '0' && *p <= '9') {
    uid = uid * 10 + (*p - '0');
    p++;
    have_uid = 1;
  }

  if (username[0] == '\0' || password[0] == '\0' || !have_uid) {
    print("usage: useradd <username> <password> <uid>\n");
    return;
  }

  if (sys_adduser(username, password, (unsigned long)uid) < 0) {
    print("useradd: failed (not root, bad name, or already exists)\n");
    return;
  }

  print("useradd: created ");
  print(username);
  print("\n");
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

/* Finds a top-level pipe separator in `line` (splitting cmd1 | cmd2).
 * Returns its index, or -1 if there isn't one. Accepts ',' as well as
 * '|': this kernel's keyboard layout has no AltGr support and every
 * key is already mapped to something else, so '|' currently can't
 * actually be typed - ',' is a typable stand-in until that's fixed.
 * Only a single pipe stage is supported for now - `a | b | c` is not
 * parsed as a 3-stage pipeline. */
static long find_pipe(const char *line) {
  for (long i = 0; line[i] != '\0'; i++) {
    if (line[i] == '|' || line[i] == ',')
      return i;
  }
  return -1;
}

static void trim(char *s) {
  unsigned long len = str_len(s);
  while (len > 0 && s[len - 1] == ' ') {
    s[--len] = '\0';
  }
  unsigned long start = 0;
  while (s[start] == ' ')
    start++;
  if (start > 0) {
    unsigned long i = 0;
    while (s[start + i] != '\0') {
      s[i] = s[start + i];
      i++;
    }
    s[i] = '\0';
  }
}

static void cmd_pipeline(char *line, long pipe_at) {
  char left[LINE_MAX];
  char right[LINE_MAX];

  unsigned long li = 0;
  for (long i = 0; i < pipe_at; i++)
    left[li++] = line[i];
  left[li] = '\0';

  unsigned long ri = 0;
  for (long i = pipe_at + 1; line[i] != '\0'; i++)
    right[ri++] = line[i];
  right[ri] = '\0';

  trim(left);
  trim(right);

  if (left[0] == '\0' || right[0] == '\0') {
    print("usage: cmd1 | cmd2\n");
    return;
  }

  long fds[2];
  if (sys_pipe(fds) < 0) {
    print("pipe: could not create\n");
    return;
  }
  long read_fd = fds[0];
  long write_fd = fds[1];

  long pid1 = sys_spawn_redirect(left, str_len(left), -1, write_fd);
  long pid2 = sys_spawn_redirect(right, str_len(right), read_fd, -1);

  /* Our own copies of the pipe fds must be closed so the pipe's
   * refcounts reflect only the two children - otherwise cmd2 would
   * never see EOF (our copy of the write end would still count as an
   * open writer) even after cmd1 exits. */
  sys_close((int)read_fd);
  sys_close((int)write_fd);

  if (pid1 < 0 || pid2 < 0) {
    print("pipe: spawn failed\n");
  }
  if (pid1 >= 0)
    sys_wait(pid1);
  if (pid2 >= 0)
    sys_wait(pid2);
}

__attribute__((section(".text._start"))) void _start(void) {
  char line[LINE_MAX];

  print("MiniKernel userspace shell\n");
  print("type a program name to run it, 'ls' to list files, 'exit' to quit\n");
  print("'write <name>' / 'cat <name>' / 'rm <name>' for persistent files\n");
  print("'chmod <name> private|readonly|public' to set who else can access it\n");
  print("'useradd <name> <password> <uid>' to create an account (root only)\n");
  print("'bg <program>' to run in the background, 'kill <pid>' to stop it\n");
  print("'cmd1 | cmd2' to pipe one program's output into another\n");
  print("(this keyboard layout has no | key yet - use ',' instead)\n");

  for (;;) {
    print("sh> ");

    unsigned long len = read_line(line, LINE_MAX);

    if (len == 0)
      continue;

    long pipe_at = find_pipe(line);
    if (pipe_at >= 0) {
      cmd_pipeline(line, pipe_at);
      continue;
    }

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

    if (str_starts_with(line, "chmod ")) {
      cmd_chmod(line + 6);
      continue;
    }

    if (str_starts_with(line, "useradd ")) {
      cmd_useradd(line + 8);
      continue;
    }

    if (str_starts_with(line, "bg ")) {
      long pid = sys_spawn(line + 3, len - 3);
      if (pid < 0) {
        print("spawn failed: ");
        print(line + 3);
        print("\n");
      } else {
        print("started in background, pid ");
        print_ulong((unsigned long)pid);
        print("\n");
      }
      continue;
    }

    if (str_starts_with(line, "kill ")) {
      long pid = 0;
      const char *p = line + 5;
      while (*p >= '0' && *p <= '9') {
        pid = pid * 10 + (*p - '0');
        p++;
      }
      if (pid <= 0) {
        print("usage: kill <pid>\n");
      } else if (sys_kill(pid, SIGKILL) < 0) {
        print("kill: no such process: ");
        print_ulong((unsigned long)pid);
        print("\n");
      } else {
        print("killed pid ");
        print_ulong((unsigned long)pid);
        print("\n");
      }
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
