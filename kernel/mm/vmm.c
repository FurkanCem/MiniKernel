#include "kernel/vmm.h"
#include "kernel/e820.h"
#include "kernel/idt.h"
#include "kernel/pmm.h"

#define PAGE_PRESENT 0x1ULL
#define PAGE_WRITABLE 0x2ULL
#define PAGE_USER 0x4ULL
#define PAGE_SIZE_2MB 0x80ULL
#define PAGE_ADDR_MASK 0x000FFFFFFFFFF000ULL

#define ENTRIES_PER_TABLE 512
#define PAGE_2MB_SIZE (2ULL * 1024 * 1024)
#define PAGE_4KB_SIZE 4096ULL

static inline unsigned int pml4_index(unsigned long long addr) {
  return (unsigned int)((addr >> 39) & 0x1FF);
}
static inline unsigned int pdpt_index(unsigned long long addr) {
  return (unsigned int)((addr >> 30) & 0x1FF);
}
static inline unsigned int pd_index(unsigned long long addr) {
  return (unsigned int)((addr >> 21) & 0x1FF);
}
static inline unsigned int pt_index(unsigned long long addr) {
  return (unsigned int)((addr >> 12) & 0x1FF);
}

static unsigned long long *kernel_pml4;

static inline void invlpg(unsigned long long addr) {
  __asm__ volatile("invlpg (%0)" ::"r"(addr) : "memory");
}

static unsigned long long *get_or_create_table(unsigned long long *table,
                                               unsigned int index) {
  if (!(table[index] & PAGE_PRESENT)) {
    unsigned long long frame = pmm_alloc_frame();
    if (frame == 0)
      return 0;

    unsigned long long *new_table = (unsigned long long *)frame;
    for (int i = 0; i < ENTRIES_PER_TABLE; i++) {
      new_table[i] = 0;
    }

    table[index] = frame | PAGE_PRESENT | PAGE_WRITABLE;
  }

  table[index] |= PAGE_USER;

  return (unsigned long long *)(table[index] & PAGE_ADDR_MASK);
}

static void map_2mb_identity(unsigned long long *pml4,
                             unsigned long long phys_addr) {
  unsigned long long *pdpt = get_or_create_table(pml4, pml4_index(phys_addr));
  if (pdpt == 0)
    return;

  unsigned long long *pd = get_or_create_table(pdpt, pdpt_index(phys_addr));
  if (pd == 0)
    return;

  unsigned int pdi = pd_index(phys_addr);
  if (!(pd[pdi] & PAGE_PRESENT)) {
    pd[pdi] = phys_addr | PAGE_PRESENT | PAGE_WRITABLE | PAGE_SIZE_2MB;
  }
}

static unsigned long long *get_pd_entry(unsigned long long *pml4,
                                        unsigned long long virt_addr) {
  if (!(pml4[pml4_index(virt_addr)] & PAGE_PRESENT))
    return 0;
  unsigned long long *pdpt =
      (unsigned long long *)(pml4[pml4_index(virt_addr)] & PAGE_ADDR_MASK);

  if (!(pdpt[pdpt_index(virt_addr)] & PAGE_PRESENT))
    return 0;
  unsigned long long *pd =
      (unsigned long long *)(pdpt[pdpt_index(virt_addr)] & PAGE_ADDR_MASK);

  return &pd[pd_index(virt_addr)];
}

static unsigned long long *get_or_split_pt_entry(unsigned long long *pml4,
                                                 unsigned long long virt_addr) {
  unsigned long long *pdpt = get_or_create_table(pml4, pml4_index(virt_addr));
  if (pdpt == 0)
    return 0;

  unsigned long long *pd = get_or_create_table(pdpt, pdpt_index(virt_addr));
  if (pd == 0)
    return 0;

  unsigned int pdi = pd_index(virt_addr);
  unsigned long long pd_entry = pd[pdi];

  unsigned long long *pt;
  if (pd_entry & PAGE_PRESENT) {
    if (pd_entry & PAGE_SIZE_2MB) {
      unsigned long long region_phys = pd_entry & PAGE_ADDR_MASK;
      unsigned long long region_flags =
          pd_entry & (PAGE_PRESENT | PAGE_WRITABLE);

      unsigned long long new_pt_frame = pmm_alloc_frame();
      if (new_pt_frame == 0)
        return 0;

      pt = (unsigned long long *)new_pt_frame;
      for (int i = 0; i < ENTRIES_PER_TABLE; i++) {
        pt[i] = (region_phys + (unsigned long long)i * PAGE_4KB_SIZE) |
                region_flags;
      }

      pd[pdi] = new_pt_frame | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;

      unsigned long long region_base = virt_addr & ~(PAGE_2MB_SIZE - 1);
      for (unsigned long long off = 0; off < PAGE_2MB_SIZE;
           off += PAGE_4KB_SIZE) {
        invlpg(region_base + off);
      }
    } else {
      pt = (unsigned long long *)(pd_entry & PAGE_ADDR_MASK);
    }
  } else {
    unsigned long long new_pt_frame = pmm_alloc_frame();
    if (new_pt_frame == 0)
      return 0;

    pt = (unsigned long long *)new_pt_frame;
    for (int i = 0; i < ENTRIES_PER_TABLE; i++) {
      pt[i] = 0;
    }

    pd[pdi] = new_pt_frame | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
  }

  pd[pdi] |= PAGE_USER;

  return &pt[pt_index(virt_addr)];
}

static unsigned long long *space_pml4(vmm_address_space_t space) {
  return (unsigned long long *)(space & PAGE_ADDR_MASK);
}

int vmm_map_page_in(vmm_address_space_t space, unsigned long long virt_addr,
                    unsigned long long phys_addr, unsigned long long flags) {
  unsigned long long *pte = get_or_split_pt_entry(space_pml4(space), virt_addr);
  if (pte == 0)
    return -1;

  *pte = (phys_addr & PAGE_ADDR_MASK) | (flags & 0xFFF) | PAGE_PRESENT;
  invlpg(virt_addr);
  return 0;
}

int vmm_unmap_page_in(vmm_address_space_t space, unsigned long long virt_addr) {
  unsigned long long *pml4 = space_pml4(space);
  unsigned long long *pd_entry = get_pd_entry(pml4, virt_addr);
  if (pd_entry == 0 || !(*pd_entry & PAGE_PRESENT))
    return 0;

  unsigned long long *pte = get_or_split_pt_entry(pml4, virt_addr);
  if (pte == 0)
    return -1;

  *pte = 0;
  invlpg(virt_addr);
  return 0;
}

int vmm_is_mapped_in(vmm_address_space_t space, unsigned long long virt_addr) {
  unsigned long long *pd_entry = get_pd_entry(space_pml4(space), virt_addr);
  if (pd_entry == 0 || !(*pd_entry & PAGE_PRESENT))
    return 0;

  if (*pd_entry & PAGE_SIZE_2MB)
    return 1;

  unsigned long long *pt = (unsigned long long *)(*pd_entry & PAGE_ADDR_MASK);
  return (pt[pt_index(virt_addr)] & PAGE_PRESENT) != 0;
}

int vmm_is_user_accessible_in(vmm_address_space_t space,
                              unsigned long long virt_addr) {
  unsigned long long *pml4 = space_pml4(space);

  unsigned long long l4 = pml4[pml4_index(virt_addr)];
  if (!(l4 & PAGE_PRESENT) || !(l4 & PAGE_USER))
    return 0;

  unsigned long long *pdpt = (unsigned long long *)(l4 & PAGE_ADDR_MASK);
  unsigned long long l3 = pdpt[pdpt_index(virt_addr)];
  if (!(l3 & PAGE_PRESENT) || !(l3 & PAGE_USER))
    return 0;

  unsigned long long *pd = (unsigned long long *)(l3 & PAGE_ADDR_MASK);
  unsigned long long l2 = pd[pd_index(virt_addr)];
  if (!(l2 & PAGE_PRESENT) || !(l2 & PAGE_USER))
    return 0;
  if (l2 & PAGE_SIZE_2MB)
    return 1;

  unsigned long long *pt = (unsigned long long *)(l2 & PAGE_ADDR_MASK);
  unsigned long long l1 = pt[pt_index(virt_addr)];
  return (l1 & PAGE_PRESENT) && (l1 & PAGE_USER);
}

void vmm_guard_page_in(vmm_address_space_t space,
                       unsigned long long virt_addr) {
  vmm_unmap_page_in(space, virt_addr);
}

void vmm_mark_user_in(vmm_address_space_t space, unsigned long long virt_addr) {
  vmm_map_page_in(space, virt_addr, virt_addr, VMM_WRITABLE | VMM_USER);
}

unsigned long long vmm_debug_walk_flags_in(vmm_address_space_t space,
                                           unsigned long long virt_addr) {
  unsigned long long *pml4 = space_pml4(space);
  unsigned long long l4 = pml4[pml4_index(virt_addr)] & 0xFFF;
  if (!(l4 & PAGE_PRESENT))
    return l4;

  unsigned long long *pdpt =
      (unsigned long long *)(pml4[pml4_index(virt_addr)] & PAGE_ADDR_MASK);
  unsigned long long l3 = pdpt[pdpt_index(virt_addr)] & 0xFFF;
  if (!(l3 & PAGE_PRESENT))
    return l4 | (l3 << 12);

  unsigned long long *pd =
      (unsigned long long *)(pdpt[pdpt_index(virt_addr)] & PAGE_ADDR_MASK);
  unsigned long long l2 = pd[pd_index(virt_addr)] & 0xFFF;
  if (!(l2 & PAGE_PRESENT) || (l2 & PAGE_SIZE_2MB))
    return l4 | (l3 << 12) | (l2 << 24);

  unsigned long long *pt =
      (unsigned long long *)(pd[pd_index(virt_addr)] & PAGE_ADDR_MASK);
  unsigned long long l1 = pt[pt_index(virt_addr)] & 0xFFF;
  return l4 | (l3 << 12) | (l2 << 24) | (l1 << 36);
}

int vmm_map_page(unsigned long long virt_addr, unsigned long long phys_addr,
                 unsigned long long flags) {
  return vmm_map_page_in(vmm_current_address_space(), virt_addr, phys_addr,
                         flags);
}

int vmm_unmap_page(unsigned long long virt_addr) {
  return vmm_unmap_page_in(vmm_current_address_space(), virt_addr);
}

int vmm_is_mapped(unsigned long long virt_addr) {
  return vmm_is_mapped_in(vmm_current_address_space(), virt_addr);
}

void vmm_guard_page(unsigned long long virt_addr) {
  vmm_guard_page_in(vmm_current_address_space(), virt_addr);
}

void vmm_mark_user(unsigned long long virt_addr) {
  vmm_mark_user_in(vmm_current_address_space(), virt_addr);
}

unsigned long long vmm_debug_walk_flags(unsigned long long virt_addr) {
  return vmm_debug_walk_flags_in(vmm_current_address_space(), virt_addr);
}

vmm_address_space_t vmm_current_address_space(void) {
  unsigned long long cr3;
  __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
  return cr3 & PAGE_ADDR_MASK;
}

vmm_address_space_t vmm_kernel_address_space(void) {
  return (unsigned long long)kernel_pml4;
}

void vmm_switch_address_space(vmm_address_space_t space) {
  __asm__ volatile("mov %0, %%cr3" : : "r"(space) : "memory");
}

#define VMM_MAX_STACK_REGIONS 16

typedef struct {
  int in_use;
  vmm_address_space_t space;
  unsigned long long stack_top;   /* exclusive upper bound, where rsp starts */
  unsigned long long stack_limit; /* inclusive lower bound the stack may grow to */
} stack_region_t;

static stack_region_t stack_regions[VMM_MAX_STACK_REGIONS];

void vmm_register_growable_stack(vmm_address_space_t space,
                                 unsigned long long stack_top,
                                 unsigned long long max_size) {
  for (int i = 0; i < VMM_MAX_STACK_REGIONS; i++) {
    if (stack_regions[i].in_use)
      continue;

    stack_regions[i].in_use = 1;
    stack_regions[i].space = space;
    stack_regions[i].stack_top = stack_top;
    stack_regions[i].stack_limit = stack_top - max_size;
    return;
  }
}

void vmm_unregister_growable_stack(vmm_address_space_t space) {
  for (int i = 0; i < VMM_MAX_STACK_REGIONS; i++) {
    if (stack_regions[i].in_use && stack_regions[i].space == space) {
      stack_regions[i].in_use = 0;
      return;
    }
  }
}

int vmm_handle_page_fault(unsigned long long fault_addr, registers_t *regs) {
  (void)regs;

  vmm_address_space_t space = vmm_current_address_space();
  unsigned long long page = fault_addr & ~(PAGE_4KB_SIZE - 1);

  for (int i = 0; i < VMM_MAX_STACK_REGIONS; i++) {
    if (!stack_regions[i].in_use || stack_regions[i].space != space)
      continue;

    if (page < stack_regions[i].stack_limit || page >= stack_regions[i].stack_top)
      continue;

    if (vmm_is_mapped_in(space, page))
      return 0; /* already backed - this is a real fault, not stack growth */

    unsigned long long frame = pmm_alloc_frame();
    if (frame == 0)
      return 0; /* out of memory - let it panic */

    if (vmm_map_page_in(space, page, frame, VMM_WRITABLE | VMM_USER) != 0) {
      pmm_free_frame(frame);
      return 0;
    }

    return 1;
  }

  return 0;
}

vmm_address_space_t vmm_create_address_space(void) {
  unsigned long long frame = pmm_alloc_frame();
  if (frame == 0)
    return 0;

  unsigned long long *new_pml4 = (unsigned long long *)frame;
  for (int i = 0; i < ENTRIES_PER_TABLE; i++) {
    new_pml4[i] = kernel_pml4[i];
  }

  return frame;
}

void vmm_init(void) {
  unsigned long long cr3;
  __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
  kernel_pml4 = (unsigned long long *)(cr3 & PAGE_ADDR_MASK);

  unsigned int count = e820_entry_count();
  for (unsigned int i = 0; i < count; i++) {
    const e820_entry_t *entry = e820_get_entry(i);
    if (entry == 0 || entry->type != E820_TYPE_USABLE)
      continue;

    unsigned long long start = entry->base & ~(PAGE_2MB_SIZE - 1);
    unsigned long long end = entry->base + entry->length;

    for (unsigned long long addr = start; addr < end; addr += PAGE_2MB_SIZE) {
      map_2mb_identity(kernel_pml4, addr);
    }
  }

  __asm__ volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");
}
