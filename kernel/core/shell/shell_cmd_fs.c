#include "kernel/klog.h"
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

  int handle = ufs_create(TEST_NAME);
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
