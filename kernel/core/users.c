#include "kernel/users.h"
#include "kernel/klog.h"
#include "kernel/ufs.h"

#define RECORD_SIZE 32
#define PASSWD_FILE ".passwd"

typedef struct {
  char username[USER_NAME_MAX];
  unsigned int uid;
  unsigned int password_hash;
} account_t;

static account_t accounts[USER_MAX_ACCOUNTS];

static unsigned int str_len(const char *s) {
  unsigned int n = 0;
  while (s[n] != '\0')
    n++;
  return n;
}

static int str_eq(const char *a, const char *b) {
  unsigned int i = 0;
  for (; a[i] != '\0' || b[i] != '\0'; i++) {
    if (a[i] != b[i])
      return 0;
  }
  return 1;
}

static unsigned int hash_password(const char *password) {
  unsigned int h = 2166136261u;
  const char *salt = "MiniKernelSalt";

  for (const char *p = salt; *p != '\0'; p++) {
    h ^= (unsigned char)*p;
    h *= 16777619u;
  }
  for (const char *p = password; *p != '\0'; p++) {
    h ^= (unsigned char)*p;
    h *= 16777619u;
  }
  return h;
}

static void write_u32(unsigned char *p, unsigned int v) {
  p[0] = (unsigned char)(v & 0xFF);
  p[1] = (unsigned char)((v >> 8) & 0xFF);
  p[2] = (unsigned char)((v >> 16) & 0xFF);
  p[3] = (unsigned char)((v >> 24) & 0xFF);
}

static unsigned int read_u32(const unsigned char *p) {
  return ((unsigned int)p[0]) | ((unsigned int)p[1] << 8) |
         ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

static void set_username(int slot, const char *name) {
  unsigned int i = 0;
  while (name[i] != '\0' && i < USER_NAME_MAX - 1) {
    accounts[slot].username[i] = name[i];
    i++;
  }
  accounts[slot].username[i] = '\0';
}

static void persist(void) {
  static unsigned char buf[USER_MAX_ACCOUNTS * RECORD_SIZE];

  for (int i = 0; i < USER_MAX_ACCOUNTS; i++) {
    unsigned char *rec = buf + i * RECORD_SIZE;
    for (int j = 0; j < USER_NAME_MAX; j++)
      rec[j] = (unsigned char)accounts[i].username[j];
    write_u32(rec + 20, accounts[i].uid);
    write_u32(rec + 24, accounts[i].password_hash);
    rec[28] = rec[29] = rec[30] = rec[31] = 0;
  }

  int handle = ufs_find(PASSWD_FILE);
  if (handle < 0) {
    handle = ufs_create(PASSWD_FILE, 0); /* owned by root */
    if (handle < 0) {
      klog_write("users: failed to create .passwd\n");
      return;
    }
    ufs_chmod(handle, 0); /* private: only root can even open it */
  } else {
    ufs_truncate(handle, 0);
  }

  ufs_write_at(handle, 0, buf, sizeof(buf));
}

static void seed_defaults(void) {
  for (int i = 0; i < USER_MAX_ACCOUNTS; i++)
    accounts[i].username[0] = '\0';

  set_username(0, "root");
  accounts[0].uid = 0;
  accounts[0].password_hash = hash_password("root");

  set_username(1, "user");
  accounts[1].uid = 1000;
  accounts[1].password_hash = hash_password("user");

  persist();
  klog_write("users: seeded default accounts (root/root, user/user)\n");
}

void users_init(void) {
  int handle = ufs_find(PASSWD_FILE);
  if (handle < 0) {
    seed_defaults();
    return;
  }

  static unsigned char buf[USER_MAX_ACCOUNTS * RECORD_SIZE];
  if (ufs_size(handle) != sizeof(buf)) {
    klog_write("users: .passwd has an unexpected size, reseeding\n");
    seed_defaults();
    return;
  }

  ufs_read_at(handle, 0, buf, sizeof(buf));
  for (int i = 0; i < USER_MAX_ACCOUNTS; i++) {
    unsigned char *rec = buf + i * RECORD_SIZE;
    for (int j = 0; j < USER_NAME_MAX; j++)
      accounts[i].username[j] = (char)rec[j];
    accounts[i].uid = read_u32(rec + 20);
    accounts[i].password_hash = read_u32(rec + 24);
  }

  klog_write("users: loaded accounts from disk\n");
}

int users_authenticate(const char *username, const char *password,
                       unsigned int *out_uid) {
  unsigned int h = hash_password(password);

  for (int i = 0; i < USER_MAX_ACCOUNTS; i++) {
    if (accounts[i].username[0] == '\0')
      continue;
    if (!str_eq(accounts[i].username, username))
      continue;

    if (accounts[i].password_hash != h)
      return -1; /* right username, wrong password */

    if (out_uid != 0)
      *out_uid = accounts[i].uid;
    return 0;
  }

  return -1; /* no such username */
}

int users_add(const char *username, const char *password, unsigned int uid) {
  if (str_len(username) == 0 || str_len(username) >= USER_NAME_MAX)
    return -1;

  for (int i = 0; i < USER_MAX_ACCOUNTS; i++) {
    if (accounts[i].username[0] != '\0' &&
        str_eq(accounts[i].username, username))
      return -1; /* already exists */
  }

  int slot = -1;
  for (int i = 0; i < USER_MAX_ACCOUNTS; i++) {
    if (accounts[i].username[0] == '\0') {
      slot = i;
      break;
    }
  }
  if (slot < 0)
    return -1; /* table full */

  set_username(slot, username);
  accounts[slot].uid = uid;
  accounts[slot].password_hash = hash_password(password);

  persist();
  return 0;
}

int users_name_for_uid(unsigned int uid, char *out, unsigned int out_len) {
  for (int i = 0; i < USER_MAX_ACCOUNTS; i++) {
    if (accounts[i].username[0] == '\0')
      continue;
    if (accounts[i].uid != uid)
      continue;

    unsigned int j = 0;
    while (accounts[i].username[j] != '\0' && j + 1 < out_len) {
      out[j] = accounts[i].username[j];
      j++;
    }
    out[j] = '\0';
    return 0;
  }
  return -1;
}
