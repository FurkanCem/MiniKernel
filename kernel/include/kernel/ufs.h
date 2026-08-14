#ifndef KERNEL_UFS_H
#define KERNEL_UFS_H

typedef struct {
  char name[20];
  unsigned int size_bytes;
} ufs_dirent_t;

void ufs_init(void);

unsigned int ufs_file_count(void);
const ufs_dirent_t *ufs_entry(unsigned int index);

int ufs_find(const char *name);
int ufs_create(const char *name);
int ufs_delete(const char *name);

unsigned int ufs_size(int handle);
int ufs_truncate(int handle, unsigned int new_size);

int ufs_write_at(int handle, unsigned int offset, const void *buf,
                 unsigned int len);
int ufs_read_at(int handle, unsigned int offset, void *buf, unsigned int len);

#endif
