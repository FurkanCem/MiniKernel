#ifndef KERNEL_VMM_STACK_H
#define KERNEL_VMM_STACK_H

#include "kernel/idt.h"
#include "kernel/vmm.h"

void vmm_register_growable_stack(vmm_address_space_t space,
                                  unsigned long long stack_top,
                                  unsigned long long max_size);
void vmm_unregister_growable_stack(vmm_address_space_t space);

int vmm_handle_page_fault(unsigned long long fault_addr, registers_t *regs);

#endif
