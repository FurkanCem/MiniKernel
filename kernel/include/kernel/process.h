#ifndef KERNEL_PROCESS_H
#define KERNEL_PROCESS_H

#include "kernel/vmm.h"

int process_spawn_from_elf(const unsigned char *elf_data,
                            unsigned long long elf_size);
int process_spawn_from_file(const char *name, const char *argv_str,
                             unsigned long long argv_len);
int process_wait(int pid, int *out_status);
void process_exit(int status) __attribute__((noreturn));
void process_mark_exited(int tid, int status);
void process_reap_thread(int tid, vmm_address_space_t space);
int process_spawn_builtin_demo(void);
int process_spawn_demo_entry(void);

#endif
