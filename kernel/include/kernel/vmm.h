#ifndef KERNEL_VMM_H
#define KERNEL_VMM_H

#include "kernel/idt.h"

#define VMM_WRITABLE 0x2ULL
#define VMM_USER 0x4ULL

typedef unsigned long long vmm_address_space_t;

void vmm_init(void);

int vmm_map_page(unsigned long long virt_addr, unsigned long long phys_addr,
                 unsigned long long flags);
int vmm_unmap_page(unsigned long long virt_addr);
int vmm_is_mapped(unsigned long long virt_addr);
void vmm_guard_page(unsigned long long virt_addr);
void vmm_mark_user(unsigned long long virt_addr);
unsigned long long vmm_debug_walk_flags(unsigned long long virt_addr);

int vmm_map_page_in(vmm_address_space_t space, unsigned long long virt_addr,
                    unsigned long long phys_addr, unsigned long long flags);
int vmm_unmap_page_in(vmm_address_space_t space, unsigned long long virt_addr);
int vmm_is_mapped_in(vmm_address_space_t space, unsigned long long virt_addr);
int vmm_is_user_accessible_in(vmm_address_space_t space,
                              unsigned long long virt_addr);
void vmm_guard_page_in(vmm_address_space_t space, unsigned long long virt_addr);
void vmm_mark_user_in(vmm_address_space_t space, unsigned long long virt_addr);
unsigned long long vmm_debug_walk_flags_in(vmm_address_space_t space,
                                           unsigned long long virt_addr);

vmm_address_space_t vmm_create_address_space(void);
void vmm_switch_address_space(vmm_address_space_t space);
vmm_address_space_t vmm_current_address_space(void);
vmm_address_space_t vmm_kernel_address_space(void);

void vmm_register_growable_stack(vmm_address_space_t space,
                                 unsigned long long stack_top,
                                 unsigned long long max_size);
void vmm_unregister_growable_stack(vmm_address_space_t space);

int vmm_handle_page_fault(unsigned long long fault_addr, registers_t *regs);

#endif
