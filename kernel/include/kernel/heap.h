#ifndef KERNEL_HEAP_H
#define KERNEL_HEAP_H

void heap_init(void);
void *kmalloc(unsigned long long size);
void kfree(void *ptr);

#endif
