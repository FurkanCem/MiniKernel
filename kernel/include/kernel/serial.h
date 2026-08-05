#ifndef KERNEL_SERIAL_H
#define KERNEL_SERIAL_H

void serial_init(void);
void serial_write(const char *str);
void serial_write_hex(unsigned long long value);

#endif
