#ifndef KERNEL_IDT_H
#define KERNEL_IDT_H

typedef struct __attribute__((packed)) {
  unsigned long long r15, r14, r13, r12, r11, r10, r9, r8;
  unsigned long long rbp, rdi, rsi, rdx, rcx, rbx, rax;
  unsigned long long vector, error_code;
  unsigned long long rip, cs, rflags, rsp, ss;
} registers_t;

void idt_init(void);

#endif
