#ifndef KERNEL_MEMFS_H
#define KERNEL_MEMFS_H

int memfs_create(const char *name);
int memfs_find(const char *name);
int memfs_delete(const char *name);

int memfs_write_at(int handle, unsigned int offset, const void *buf,
                    unsigned int len);
int memfs_read_at(int handle, unsigned int offset, void *buf,
                   unsigned int len);
unsigned int memfs_size(int handle);

int memfs_first(void);
int memfs_next(int handle);
const char *memfs_name(int handle);
unsigned int memfs_file_count(void);

#endif
