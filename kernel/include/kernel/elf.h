#ifndef KERNEL_ELF_H
#define KERNEL_ELF_H

#include "kernel/vmm.h"

int elf_load(vmm_address_space_t space, const unsigned char *data,
             unsigned long long size, unsigned long long load_bias,
             unsigned long long *out_entry);

#endif
