#ifndef KERNEL_VMM_H
#define KERNEL_VMM_H

#define VMM_WRITABLE 0x2ULL
#define VMM_USER 0x4ULL /* required for ANY ring-3 access to this page */

void vmm_init(void);

int vmm_map_page(unsigned long long virt_addr, unsigned long long phys_addr,
                 unsigned long long flags);
int vmm_unmap_page(unsigned long long virt_addr);
int vmm_is_mapped(unsigned long long virt_addr);

unsigned long long vmm_debug_walk_flags(unsigned long long virt_addr);

void vmm_guard_page(unsigned long long virt_addr);

void vmm_mark_user(unsigned long long virt_addr);

#endif
