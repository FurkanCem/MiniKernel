#ifndef KERNEL_FS_H
#define KERNEL_FS_H

typedef struct {
  char name[20];
  unsigned int start_lba;
  unsigned int size_bytes;
} fs_dirent_t;

void fs_init(void);
unsigned int fs_file_count(void);
const fs_dirent_t *fs_entry(unsigned int index);
const fs_dirent_t *fs_find(const char *name);
int fs_read_file(const fs_dirent_t *entry, void *buf);

#endif
