#ifndef KERNEL_ELF_H
#define KERNEL_ELF_H

#include "kernel/vmm.h"

extern const unsigned char tiny_elf_binary[];
extern const unsigned long long tiny_elf_binary_size;

int elf_load(vmm_address_space_t space, const unsigned char *data,
             unsigned long long size, unsigned long long *out_entry);

#endif
