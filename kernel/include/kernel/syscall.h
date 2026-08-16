#ifndef KERNEL_SYSCALL_H
#define KERNEL_SYSCALL_H

#include "kernel/idt.h"

#define SYS_WRITE_HELLO 1
#define SYS_EXIT 2
#define SYS_WRITE 3
#define SYS_READ 4
#define SYS_SPAWN 5
#define SYS_WAIT 6
#define SYS_OPEN 7
#define SYS_CLOSE 8
#define SYS_LIST 9
#define SYS_WHOAMI 10
#define SYS_MEMINFO 11
#define SYS_REMOVE 12
#define SYS_CLEAR 13
#define SYS_SET_CURSOR 14
#define SYS_DRAW_ROW 15
#define SYS_SCREEN_INFO 16
#define SYS_KILL 17
#define SYS_PIPE 18
#define SYS_SPAWN_REDIRECT 19
#define SYS_GETUID 20
#define SYS_CHMOD 21
#define SYS_LOGIN 22
#define SYS_ADDUSER 23
#define SYS_USERNAME 24

void syscall_handler(registers_t *regs);

#endif
