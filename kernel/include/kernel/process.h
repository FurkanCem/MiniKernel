#ifndef KERNEL_PROCESS_H
#define KERNEL_PROCESS_H

int process_spawn_from_elf(const unsigned char *elf_data,
                            unsigned long long elf_size);
int process_spawn_from_file(const char *name, const char *argv_str,
                             unsigned long long argv_len);
int process_spawn_builtin_demo(void);
int process_spawn_demo_entry(void);

#endif
