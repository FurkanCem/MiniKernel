#ifndef KERNEL_PIPE_H
#define KERNEL_PIPE_H

int pipe_create(void);

int pipe_add_reader(int idx);
int pipe_add_writer(int idx);

void pipe_close_reader(int idx);
void pipe_close_writer(int idx);

int pipe_read(int idx, void *buf, unsigned int len);
int pipe_write(int idx, const void *buf, unsigned int len);

#endif
