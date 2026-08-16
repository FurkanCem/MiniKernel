#ifndef KERNEL_FD_H
#define KERNEL_FD_H

#define FD_CREATE 1ULL
#define FD_PERSIST 2ULL
#define FD_TRUNC 4ULL

int fd_open(const char *name, unsigned long long flags);
int fd_close(int fd);
long fd_read(int fd, void *buf, unsigned long long len);
long fd_write(int fd, const void *buf, unsigned long long len);
void fd_cleanup_thread(int tid);

#endif
