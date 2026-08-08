#include "kernel/vmm.h"
#include "kernel/e820.h"
#include "kernel/pmm.h"

#define PAGE_PRESENT 0x1ULL
#define PAGE_WRITABLE 0x2ULL
#define PAGE_SIZE_2MB 0x80ULL /* PS bit - only meaningful at the PD level */
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

static unsigned long long *pml4;

static inline void invlpg(unsigned long long addr) {
  __asm__ volatile("invlpg (%0)" ::"r"(addr) : "memory");
}

static unsigned long long *get_or_create_table(unsigned long long *table,
                                               unsigned int index) {
  if (!(table[index] & PAGE_PRESENT)) {
    unsigned long long frame = pmm_alloc_frame();
    if (frame == 0)
      return 0; /* out of memory - caller must handle */

    unsigned long long *new_table = (unsigned long long *)frame;
    for (int i = 0; i < ENTRIES_PER_TABLE; i++) {
      new_table[i] = 0;
    }

    table[index] = frame | PAGE_PRESENT | PAGE_WRITABLE;
  }

  return (unsigned long long *)(table[index] & PAGE_ADDR_MASK);
}

static void map_2mb_identity(unsigned long long phys_addr) {
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

static unsigned long long *get_pd_entry(unsigned long long virt_addr) {
  unsigned long long *l4 = pml4;
  if (!(l4[pml4_index(virt_addr)] & PAGE_PRESENT))
    return 0;
  unsigned long long *pdpt =
      (unsigned long long *)(l4[pml4_index(virt_addr)] & PAGE_ADDR_MASK);

  if (!(pdpt[pdpt_index(virt_addr)] & PAGE_PRESENT))
    return 0;
  unsigned long long *pd =
      (unsigned long long *)(pdpt[pdpt_index(virt_addr)] & PAGE_ADDR_MASK);

  return &pd[pd_index(virt_addr)];
}

static unsigned long long *get_or_split_pt_entry(unsigned long long virt_addr) {
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

      pd[pdi] = new_pt_frame | PAGE_PRESENT | PAGE_WRITABLE;

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

    pd[pdi] = new_pt_frame | PAGE_PRESENT | PAGE_WRITABLE;
  }

  return &pt[pt_index(virt_addr)];
}

int vmm_map_page(unsigned long long virt_addr, unsigned long long phys_addr,
                 unsigned long long flags) {
  unsigned long long *pte = get_or_split_pt_entry(virt_addr);
  if (pte == 0)
    return -1;

  *pte = (phys_addr & PAGE_ADDR_MASK) | (flags & 0xFFF) | PAGE_PRESENT;
  invlpg(virt_addr);
  return 0;
}

int vmm_unmap_page(unsigned long long virt_addr) {
  unsigned long long *pd_entry = get_pd_entry(virt_addr);
  if (pd_entry == 0 || !(*pd_entry & PAGE_PRESENT))
    return 0;

  unsigned long long *pte = get_or_split_pt_entry(virt_addr);
  if (pte == 0)
    return -1;

  *pte = 0; /* PAGE_PRESENT cleared - any access now takes a #PF */
  invlpg(virt_addr);
  return 0;
}

int vmm_is_mapped(unsigned long long virt_addr) {
  unsigned long long *pd_entry = get_pd_entry(virt_addr);
  if (pd_entry == 0 || !(*pd_entry & PAGE_PRESENT))
    return 0;

  if (*pd_entry & PAGE_SIZE_2MB)
    return 1; /* whole 2MB leaf present, no need to split just to check */

  unsigned long long *pt = (unsigned long long *)(*pd_entry & PAGE_ADDR_MASK);
  return (pt[pt_index(virt_addr)] & PAGE_PRESENT) != 0;
}

void vmm_guard_page(unsigned long long virt_addr) { vmm_unmap_page(virt_addr); }

void vmm_init(void) {
  unsigned long long cr3;
  __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
  pml4 = (unsigned long long *)(cr3 & PAGE_ADDR_MASK);

  unsigned int count = e820_entry_count();
  for (unsigned int i = 0; i < count; i++) {
    const e820_entry_t *entry = e820_get_entry(i);
    if (entry == 0 || entry->type != E820_TYPE_USABLE)
      continue;

    unsigned long long start = entry->base & ~(PAGE_2MB_SIZE - 1);
    unsigned long long end = entry->base + entry->length;

    for (unsigned long long addr = start; addr < end; addr += PAGE_2MB_SIZE) {
      map_2mb_identity(addr);
    }
  }

  __asm__ volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");
}
