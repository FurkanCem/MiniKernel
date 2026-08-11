#ifndef SHELL_INTERNAL_H
#define SHELL_INTERNAL_H

int str_eq(const char *a, const char *b);
int str_starts_with(const char *str, const char *prefix);

void print_str(const char *str);
void print_hex(unsigned long long value);

void cmd_meminfo(void);
void cmd_alloctest(void);
void cmd_vmmtest(void);
void cmd_heaptest(void);

void cmd_threadtest(void);
void cmd_largethreadtest(void);

void cmd_usermodetest(void);
void cmd_elftest(void);
void cmd_ls(void);
void cmd_run(const char *name);

#endif
