#include "kernel/klog.h"
#include "kernel/serial.h"

void klog_init(void){
    serial_init();
}

void klog_write(const char *str){
    serial_write(str);
}

void klog_write_n(const char *data, unsigned long long len){
    serial_write_n(data, len);
}

void klog_write_hex(unsigned long long value){
    serial_write_hex(value);
}
