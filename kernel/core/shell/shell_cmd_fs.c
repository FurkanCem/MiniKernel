#include "kernel/fd.h"
#include "kernel/klog.h"
#include "kernel/process.h"
#include "kernel/thread.h"
#include "kernel/ufs.h"
#include "kernel/video.h"
#include "shell_internal.h"

#define TEST_NAME "selftest"
#define TEST_CONTENT "MiniKernel persistent storage works!"

static int str_eq_local(const char *a, const char *b) {
  unsigned int i = 0;
  for (; a[i] != '\0' || b[i] != '\0'; i++) {
    if (a[i] != b[i])
      return 0;
  }
  return 1;
}

static void report(const char *label, int ok) {
  print_str("\n  ");
  print_str(label);
  print_str(ok ? ": PASS" : ": FAIL");

  klog_write("ufstest: ");
  klog_write(label);
  klog_write(ok ? ": PASS\n" : ": FAIL\n");
}

void cmd_ufstest(void) {
  print_str("\nexercising the persistent (disk-backed) filesystem:");
  klog_write("ufstest: starting\n");

  unsigned int content_len = 0;
  while (TEST_CONTENT[content_len] != '\0')
    content_len++;

  /* Start from a clean slate in case this ran before during this boot. */
  ufs_delete(TEST_NAME);

  int handle =
      ufs_create(TEST_NAME, 0); /* uid 0 = root, this is a kernel-mode test */
  report("create", handle >= 0);
  if (handle < 0)
    return;

  int written = ufs_write_at(handle, 0, TEST_CONTENT, content_len);
  report("write", written == (int)content_len);

  char readback[64];
  for (unsigned int i = 0; i < sizeof(readback); i++)
    readback[i] = 0;
  int got = ufs_read_at(handle, 0, readback, content_len);
  report("read-back",
         got == (int)content_len && str_eq_local(readback, TEST_CONTENT));

  ufs_init();

  int handle2 = ufs_find(TEST_NAME);
  report("survives remount (found)", handle2 >= 0);

  if (handle2 >= 0) {
    for (unsigned int i = 0; i < sizeof(readback); i++)
      readback[i] = 0;
    int got2 = ufs_read_at(handle2, 0, readback, content_len);
    report("survives remount (content)",
           got2 == (int)content_len && str_eq_local(readback, TEST_CONTENT));
  }

  print_str("\ndone - '");
  print_str(TEST_NAME);
  print_str("' left on disk; reboot and run 'ufstest' again, or check "
            "it from the user shell with 'cat ");
  print_str(TEST_NAME);
  print_str("', to confirm it survives a real power cycle too.");

  klog_write("ufstest: finished\n");
}

void cmd_setuid(const char *args) {
  while (*args == ' ')
    args++;

  int pid = 0;
  int any_pid_digit = 0;
  while (*args >= '0' && *args <= '9') {
    pid = pid * 10 + (*args - '0');
    args++;
    any_pid_digit = 1;
  }

  while (*args == ' ')
    args++;

  unsigned int uid = 0;
  int any_uid_digit = 0;
  while (*args >= '0' && *args <= '9') {
    uid = uid * 10 + (unsigned int)(*args - '0');
    args++;
    any_uid_digit = 1;
  }

  if (!any_pid_digit || !any_uid_digit) {
    print_str("\nusage: setuid <pid> <uid>");
    return;
  }

  int tid = process_get_tid(pid);
  if (tid < 0) {
    print_str("\nsetuid: no such process");
    return;
  }

  sched_set_uid(tid, uid);

  char digits[12];
  int n = 0;
  unsigned int v = uid;
  if (v == 0) {
    digits[n++] = '0';
  } else {
    while (v > 0) {
      digits[n++] = (char)('0' + v % 10);
      v /= 10;
    }
  }
  char msg[32];
  int m = 0;
  const char *prefix = "\nsetuid: pid now runs as uid ";
  while (prefix[m] != '\0') {
    msg[m] = prefix[m];
    m++;
  }
  while (n > 0)
    msg[m++] = digits[--n];
  msg[m] = '\0';
  print_str(msg);
}

#define PERM_TEST_NAME "permtest_secret"

static void perm_report(const char *label, int ok) {
  print_str("\n  ");
  print_str(label);
  print_str(ok ? ": PASS" : ": FAIL");

  klog_write("permtest: ");
  klog_write(label);
  klog_write(ok ? ": PASS\n" : ": FAIL\n");
}

void cmd_permtest(void) {
  print_str("\nexercising UFS owner/permission checks:");
  klog_write("permtest: starting\n");

  int tid = sched_current_tid();
  unsigned int original_uid = sched_get_uid(tid);

  ufs_delete(PERM_TEST_NAME); /* clean slate if this ran before */

  sched_set_uid(tid, 1000);
  int fd = fd_open(PERM_TEST_NAME, FD_CREATE | FD_PERSIST);
  perm_report("owner (1000) can create", fd >= 0);
  if (fd >= 0) {
    long n = fd_write(fd, "hello", 5);
    perm_report("owner (1000) can write", n == 5);
    fd_close(fd);
  }

  sched_set_uid(tid, 2000);
  int fd_other = fd_open(PERM_TEST_NAME, FD_PERSIST);
  perm_report("non-owner (2000) denied open-for-read by default", fd_other < 0);

  sched_set_uid(tid, 1000);
  int handle = ufs_find(PERM_TEST_NAME);
  perm_report("chmod to allow others to read",
              handle >= 0 && ufs_chmod(handle, UFS_PERM_OTHER_READ) == 0);

  sched_set_uid(tid, 2000);
  fd_other = fd_open(PERM_TEST_NAME, FD_PERSIST);
  perm_report("non-owner (2000) can now read", fd_other >= 0);
  if (fd_other >= 0) {
    char buf[8];
    long n = fd_read(fd_other, buf, 5);
    perm_report("non-owner (2000) read gets real content", n == 5);

    long w = fd_write(fd_other, "nope!", 5);
    perm_report("non-owner (2000) still denied write (read-only)", w < 0);
    fd_close(fd_other);
  }

  sched_set_uid(tid, 1000);
  handle = ufs_find(PERM_TEST_NAME);
  perm_report("chmod to allow others to write too",
              handle >= 0 && ufs_chmod(handle, UFS_PERM_OTHER_READ |
                                                   UFS_PERM_OTHER_WRITE) == 0);

  sched_set_uid(tid, 2000);
  fd_other = fd_open(PERM_TEST_NAME, FD_PERSIST);
  if (fd_other >= 0) {
    long w = fd_write(fd_other, "yep!!", 5);
    perm_report("non-owner (2000) can now write (public)", w == 5);
    fd_close(fd_other);
  } else {
    perm_report("non-owner (2000) can now write (public)", 0);
  }

  sched_set_uid(tid, 3000);
  handle = ufs_find(PERM_TEST_NAME);
  int delete_denied =
      handle < 0 || ufs_owner(handle) != sched_get_uid(sched_current_tid());
  perm_report("non-owner (3000) denied delete regardless of perm bits",
              delete_denied);

  sched_set_uid(tid, original_uid);

  print_str("\ndone - '");
  print_str(PERM_TEST_NAME);
  print_str("' left on disk, owned by uid 1000, world-writable.");

  klog_write("permtest: finished\n");
}
