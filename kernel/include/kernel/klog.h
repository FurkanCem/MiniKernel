#ifndef KERNEL_KLOG_H
#define KERNEL_KLOG_H

void klog_init(void);
void klog_write(const char *str);
void klog_write_hex(unsigned long long value);

#endif
