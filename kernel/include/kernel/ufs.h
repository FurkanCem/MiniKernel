#ifndef KERNEL_UFS_H
#define KERNEL_UFS_H

#define UFS_PERM_OTHER_READ 0x1
#define UFS_PERM_OTHER_WRITE 0x2

typedef struct {
  char name[20];
  unsigned int size_bytes;
  unsigned int owner_uid;
  unsigned char perm;
} ufs_dirent_t;

void ufs_init(void);

unsigned int ufs_file_count(void);
const ufs_dirent_t *ufs_entry(unsigned int index);

int ufs_find(const char *name);
int ufs_create(const char *name, unsigned int owner_uid);
int ufs_delete(const char *name);

unsigned int ufs_size(int handle);
unsigned int ufs_owner(int handle);
unsigned char ufs_perm(int handle);
int ufs_chmod(int handle, unsigned char perm);
int ufs_truncate(int handle, unsigned int new_size);

int ufs_write_at(int handle, unsigned int offset, const void *buf,
                 unsigned int len);
int ufs_read_at(int handle, unsigned int offset, void *buf, unsigned int len);

#endif
